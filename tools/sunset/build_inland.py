"""Export the authored inland route and its component meshes as glTF."""
import json
import math
from pathlib import Path
import random

from build_architecture import Model


COLORS = {
    "meadow": (.68, 1., .53, 1.), "silt": (.76, .72, .60, 1.),
    "track": (1., .89, .74, 1.), "deck": (.85, .77, .66, 1.),
    "grass": (.19, .29, .079, 1.), "seed": (.33, .34, .11, 1.),
    "reeds": (.22, .31, .115, 1.), "rust": (.90, .73, .61, 1.),
    "sheet": (.70, .78, .80, 1.), "concrete": (.51, .49, .45, 1.),
    "metal": (.30, .34, .35, 1.), "glass": (.20, .25, .27, 1.),
    "tread": (.59, .31, .09, 1.), "lamp": (1., .67, .30, 1.),
    "roadbed": (.21, .23, .23, 1.),
    "sky": (1., 1., 1., 1.), "water": (1., 1., 1., 1.),
}
TEXTURES = {"meadow": "sparse_grass", "silt": "mud_forest", "track": "mud_forest",
            "deck": "wood_planks_grey", "rust": "rusty_corrugated_iron", "sheet": "corrugated_iron"}


def route_at(keys, z):
    for first, last in zip(keys, keys[1:]):
        if z <= last[0]:
            weight = max(0., (z-first[0])/(last[0]-first[0]))
            return tuple(a+(b-a)*weight for a,b in zip(first[1:], last[1:]))
    return keys[-1][1:]


def pond(x, z):
    center = -5. + 10.*math.sin(.091*(z-64.))
    return ((x-center)/26.)**2 + ((z-78.)/28.)**2


def terrain_y(keys, hills, x, z):
    center, track = route_at(keys, z)
    distance = abs(x-center)
    wave = 1.85*math.sin(x*.069+z*.036)+.72*math.cos(x*.127-z*.092)
    meadow = track-.09+min(1., max(0., (distance-2.)/15.))*wave
    meadow += 5.5*max(0., min(1., (distance-29.)/33.))*max(0., 1.-abs(z-16.)/58.)
    marsh = min(1.3, 1.15*(pond(x,z)-1.))
    blend = max(0., min(1., (z-42.)/14., (115.-z)/13.))
    yield_to_yard = max(0., min(1., (z-105.)/12.))
    relief = sum(height * math.exp(-((x-a)/width)**2-((z-b)/length)**2)
                 for a,b,width,length,height in hills)
    relief *= max(0., min(1., (distance-3.)/8., (pond(x,z)-1.)*.8))
    if z > 108.:
        relief *= max(min(1., (z-187.)/29.), max(0., min(1., (distance-28.)/19.)))
    return (meadow*(1.-blend)+marsh*blend)*(1.-yield_to_yard)+.89*yield_to_yard+relief


def surface(model, part, xs, zs, point, uv_scale):
    for z0,z1 in zip(zs,zs[1:]):
        for x0,x1 in zip(xs,xs[1:]):
            quad = [point(x,z) for x,z in ((x0,z0),(x0,z1),(x1,z1),(x1,z0))]
            for a,b,c in ((0,1,2),(0,2,3)):
                points = [quad[a],quad[b],quad[c]]
                model.face(part, points, [(p[0]*uv_scale,p[2]*uv_scale) for p in points])


def vegetation(keys, hills):
    model = Model()
    rng = random.Random(7159)
    for index in range(5410):
        x,z = rng.uniform(-51.,48.),rng.uniform(-22.,111.)
        if index < 2280:
            z = rng.uniform(-23.,49.)
            x = route_at(keys,z)[0]+rng.uniform(-20.,20.)
        if abs(x-route_at(keys,z)[0]) < 2.45:
            continue
        y = terrain_y(keys,hills,x,z)
        marsh = 51.<z<106.
        if (marsh and not -.36 < y < .47) or (not marsh and rng.random() < .17):
            continue
        for blade in range(rng.randrange(4,8) if marsh else rng.randrange(3,6)):
            angle = rng.uniform(0.,math.tau)
            width = rng.uniform(.021,.066) if marsh else rng.uniform(.017,.040)
            dx,dz = math.cos(angle),math.sin(angle)
            height = rng.uniform(.63,1.46) if marsh else rng.uniform(.15,.52)
            ox,oz = x+rng.uniform(-.21,.21),z+rng.uniform(-.21,.21)
            bend = rng.uniform(.14,.52)*height
            base_a = (ox-dx*width,y-.055,oz-dz*width)
            base_b = (ox+dx*width,y-.055,oz+dz*width)
            middle_a = (ox-dx*width*.51-dz*bend*.2,y+height*.56,oz-dz*width*.51+dx*bend*.2)
            middle_b = (ox+dx*width*.51-dz*bend*.2,y+height*.56,oz+dz*width*.51+dx*bend*.2)
            tip = (ox-dz*bend,y+height,oz+dx*bend)
            kind = "reeds" if marsh else ("seed" if blade==0 else "grass")
            model.face(kind,[base_a,middle_a,middle_b,base_b],[(0.,0.),(0.,.56),(1.,.56),(1.,0.)])
            model.face(kind,[middle_a,tip,middle_b],[(0.,.56),(.5,1.),(1.,.56)])
    return model


def path_model(keys):
    model = Model()
    def point(x,z):
        center, height = route_at(keys,z)
        return center+x,height,z
    for begin,end in [(-26,53),(101,182)]:
        steps = sorted(set(range(begin,end+1,2)) | {end} | {k[0] for k in keys if begin<k[0]<end})
        surface(model, "track", [-2.1,0,2.1], steps, point, .51)
    for board in range(160):
        z = 53.+board*.3
        center,height = route_at(keys,z)
        model.box("deck",center-2.1,height-.105,z,4.2,.105,.296)
        if board%10==0:
            for sign in (-1,1):
                x = center+2.02*sign
                model.box("deck",x-.08,-.56,z,.16,1.91,.16)
                zz = z+3.
                if zz<101.:
                    xx = route_at(keys,zz)[0]+2.02*sign
                    model.tube("metal",(x,1.15,z+.08),(xx,1.15,zz+.08),.039,5)
    return model


def factory(layout):
    model, lamps = Model(), Model()
    model.box("roadbed",-5.4,.896,117.,11.6,.085,63.)
    for z in range(119,178,4):
        for x in (-2.9,3.5):
            model.box("tread",x,.982,z,.09,.013,1.3)
    for x,z,w,d,wall in layout["shed"]:
        floor, eave, ridge = .88, .88+wall, .88+wall+w*.14
        model.box("concrete",x-.23,.64,z-.25,w+.46,.53,d+.5)
        model.box("sheet",x,floor,z,.18,wall,d)
        model.box("rust",x+w-.18,floor,z,.18,wall,d)
        model.box("sheet",x,floor,z+d-.12,w,wall,.12)
        model.box("rust",x,floor,z,w,wall,.12)
        model.box("metal",x+w*.28,1.17,z-.093,w*.44,wall*.68,.14)
        for slat in range(1,12):
            model.box("sheet",x+w*.28,1.17+slat*wall*.053,z-.107,w*.44,.06,.04)
        front, rear = z-.52,z+d+.5
        left = [(x-.55,eave,front),(x-.55,eave,rear),(x+w*.5,ridge,rear),(x+w*.5,ridge,front)]
        right = [(x+w*.5,ridge,front),(x+w*.5,ridge,rear),(x+w+.55,eave,rear),(x+w+.55,eave,front)]
        for slope in (left,right):
            model.face("sheet",slope)
        for end in (z,z+d):
            model.face("rust",[(x,eave,end),(x+w*.5,ridge,end),(x+w,eave,end)])
        for offset in (.12,.86):
            window = x+w*offset
            model.box("metal",window-.86,eave-1.78,z-.13,1.72,1.16,.12)
            model.box("glass",window-.77,eave-1.69,z-.16,1.54,.98,.08)
            for pane in (-.39,0.,.39):
                model.box("metal",window+pane,eave-1.70,z-.21,.045,1.04,.08)
        model.box("metal",x+w*.5-.26,eave-.37,z-.20,.52,.28,.32)
        lamps.box("lamp",x+w*.5-.20,eave-.39,z-.50,.40,.12,.24)
        for n in range(1,5):
            side_z = z+n*d/5
            model.box("metal",x-.06,floor,side_z,.29,wall,.25)
            model.box("metal",x+w-.22,floor,side_z,.29,wall,.25)
            for edge in (x-.12,x+w+.06):
                model.box("concrete",edge,eave-1.62,side_z+.41,.071,.81,1.49)
                for louver in range(4):
                    model.box("metal",edge-.03,eave-1.52+louver*.16,side_z+.48,.13,.06,1.32)
    for x,z,radius,height in layout["tanks"]:
        model.box("concrete",x-radius-1.,.74,z-radius-1.,2.*radius+2.,.36,2.*radius+2.)
        model.tube("sheet",(x,1.1,z),(x,height,z),radius,28)
        for layer in (.17,.36,.55,.74,.94):
            y=height*layer+1.
            model.tube("metal",(x,y,z),(x,y+.08,z),radius+.047,28)
        for side in range(28):
            a,b = side*math.tau/28,(side+1)*math.tau/28
            model.face("rust",[(x,height+.65,z),(x+radius*math.cos(b),height,z+radius*math.sin(b)),
                               (x+radius*math.cos(a),height,z+radius*math.sin(a))])
        for offset in (-.31,.31):
            model.tube("metal",(x+offset,1.2,z-radius-.12),(x+offset,height+.65,z-radius-.12),.05,6)
        for rung in range(1,int(height/.32)):
            y=1.+rung*.32
            model.tube("tread",(x-.34,y,z-radius-.12),(x+.34,y,z-radius-.12),.031,6)
    for x,z,radius,height in layout["stacks"]:
        model.tube("rust",(x,.89,z),(x,height,z),radius,24)
        for fraction in (.25,.50,.73,.89,.98):
            y=height*fraction
            model.tube("sheet",(x,y,z),(x,y+.38,z),radius+.09,24)
        lamps.box("lamp",x-.13,height+.03,z-.13,.26,.26,.26)
    for x in (-8.,7.):
        model.box("concrete",x-.55,.86,134.,1.10,.31,1.10)
        model.tube("metal",(x,1.1,134.5),(x,7.9,134.5),.24,10)
    for height in (6.7,7.48):
        model.tube("rust",(-19.,height,134.5),(17.,height,134.5),.27,14)
        for x in (-16.,-8.,-.4,7.,13.):
            model.tube("sheet",(x,height,134.5),(x+.19,height,134.5),.33,14)
    for z in (115.,144.,165.):
        for x in (-4.8,6.1):
            model.tube("metal",(x,.9,z),(x,5.1,z),.095,8)
            model.tube("metal",(x,5.1,z),(x+.71,5.1,z),.07,8)
            lamps.box("lamp",x+.53,4.98,z-.13,.42,.13,.27)
    for x,z in [(-5.3,120.),(5.4,141.),(-6.4,150.)]:
        model.box("metal",x,.91,z,1.15,1.25,.62)
        model.box("sheet",x+.075,1.12,z-.07,1.0,.89,.13)
        model.box("tread",x+.68,1.52,z-.112,.25,.14,.05)
    return model, lamps


def build_inland(root):
    spec = json.loads(Path(__file__).with_name("inland-layout.json").read_text(encoding="utf-8"))
    keys, hills = spec["road"], spec["hills"]
    ground = Model()
    elevation = lambda x,z: (x,terrain_y(keys,hills,x,z),z)
    xs = range(-132,133,3)
    for material,start,end in [("meadow",-66,48),("silt",48,114),("track",114,189),("meadow",189,321)]:
        surface(ground,material,xs,range(start,end+1,3),elevation,.36)
    water = Model()
    point = lambda x,z: (x,0.,z)
    for z in range(49,108,2):
        for x in range(-43,42,2):
            quad = [point(x,z),point(x,z+2),point(x+2,z+2),point(x+2,z)]
            water.face("water",quad,[(max(0.,-terrain_y(keys,hills,a,c)),0.) for a,_,c in quad])
    sky = Model()
    sky.box("sky",-310.,-310.,-310.,620.,620.,620.)
    yard, bulbs = factory(spec)
    for name,model in (("inland_ground",ground),("inland_path",path_model(keys)),("inland_grasses",vegetation(keys,hills)),
                       ("inland_yard",yard),("inland_lamps",bulbs),("inland_water",water),("inland_sky",sky)):
        model.write(root,name,COLORS,TEXTURES)
    (root/"route.json").write_text(json.dumps({"road": keys}),encoding="utf-8")
