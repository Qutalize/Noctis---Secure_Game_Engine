"""Export the authored road corridor, layered mesas and roadside props."""
import json
import math
from pathlib import Path
import random

from build_architecture import Model


SOIL = "terrain_red_01"
ASPHALT = "worn_asphalt"
COLORS = {SOIL: (1., .90, .74, 1.), ASPHALT: (.65, .70, .74, 1.),
          "sky": (1., 1., 1., 1.), "yellow": (.91, .61, .117, 1.),
          "line": (.76, .75, .69, 1.), "rock": (.51, .27, .13, 1.),
          "red": (1., .62, .37, 1.), "ochre": (1., .92, .67, 1.),
          "band": (.71, .44, .28, 1.), "stem": (.20, .13, .076, 1.),
          "leaf": (.29, .30, .13, 1.), "metal": (.45, .42, .37, 1.),
          "timber": (.17, .103, .059, 1.)}
VEGETATION_SEED = 48261
MESA_COLORS = {**COLORS, "rock": (.91,.72,.52,1.)}


def road_at(keys, distance):
    for first, last in zip(keys, keys[1:]):
        if distance <= last[0]:
            weight = min(1., max(0., (distance-first[0])/(last[0]-first[0])))
            return tuple(a+(b-a)*weight for a,b in zip(first[1:],last[1:]))
    return tuple(keys[-1][1:])


def ground_at(keys, x, z):
    axis, elevation = road_at(keys, z)
    distance = abs(x-axis)
    blend = min(1., max(0., (distance-4.95)/15.))
    hills = math.sin(x*.110+z*.049)*.66 + math.sin(z*.133-x*.027)*.57
    wash = math.exp(-((x+18.-.12*z-5.*math.sin(z*.061))/6.5)**2)
    return elevation-.082+blend*(hills-1.15*wash+.28*math.sin(x*.36-z*.23))


def build_ground(keys):
    model = Model()
    offsets = [-185.,-138.,-101.,-80.,-65.,-54.,-47.,-40.,-34.,-29.,-24.,-19.,-15.,-12.,-9.,
               -7.,-5.9,-4.95,-4.25,-3.8,0.,3.8,4.25,4.95,5.9,7.,9.,12.,15.,19.,24.,29.,34.,
               40.,47.,54.,65.,80.,101.,138.,185.]

    def point(offset, z, lift=0.):
        x, height = road_at(keys,z)
        return x+offset,height+lift,z

    def land(offset,z):
        x,_,_ = point(offset,z)
        return x,ground_at(keys,x,z),z

    def strip(kind, left,right,near,far,lift=0.):
        points = [point(left,far,lift),point(right,far,lift),point(right,near,lift),point(left,near,lift)]
        uv = [(offset/4.4,z/7.5) for offset,z in ((left,far),(right,far),(right,near),(left,near))]
        model.face(kind,points,uv)

    for distance in range(-32,409,2):
        following=min(distance+2,409)
        for left,right in zip(offsets,offsets[1:]):
            edge = [land(left,following),land(right,following),land(right,distance),land(left,distance)]
            model.face(SOIL,edge,[(x/4.6,z/4.6) for x,_,z in edge])
        strip(ASPHALT,-4.12,4.12,distance,following)
        for side in (-1,1):
            stripe=side*3.88
            strip("line",stripe-.047,stripe+.047,distance,following,.018)
    for distance in range(-30,406,8):
        for side in (-1,1):
            center=side*.15
            strip("yellow",center-.055,center+.055,distance,distance+3.4,.019)
    return model


def build_mesas(pieces, keys):
    model=Model()
    levels=((0,1.13),(.11,1.0),(.31,.79),(.40,.74),(.46,.60),(.57,.61),
            (.64,.55),(.735,.54),(.79,.50),(.86,.49),(.94,.44),(1.,.42))
    bands=("ochre","rock","red","band","red","rock","band","red","ochre","red","rock")
    sides=47
    for x,z,width,depth,height,seed in pieces:
        base=ground_at(keys,x,z)-.92
        rings=[]
        for index,(elevation,radius) in enumerate(levels):
            row=[]
            for corner in range(sides):
                angle=math.tau*corner/sides
                erosion=(1. + .125*math.sin(angle*5+seed) + .083*math.sin(angle*9-seed*3) +
                         .079*math.sin(angle*3+index*.78+seed)+.042*math.sin(angle*17+index*.63+seed))
                crest=.028*math.sin(angle*3+seed)+.011*math.sin(angle*8+seed*4)
                row.append((x+math.cos(angle)*width*radius*erosion+elevation*math.sin(seed)*3.4,
                            base+height*(elevation+crest),
                            z+math.sin(angle)*depth*radius*erosion+elevation*math.cos(seed)*2.1))
            rings.append(row)
        for index,(lower,upper) in enumerate(zip(rings,rings[1:])):
            for corner in range(sides):
                next_corner=(corner+1)%sides
                points=[lower[corner],upper[corner],upper[next_corner],lower[next_corner]]
                model.face(bands[index],points,
                           [(point[0]*.115+point[2]*.147,-point[1]*.157) for point in points])
        for corner in range(sides):
            model.face("ochre",[(x+math.sin(seed)*3.4,base+height*.985,z),
                                 rings[-1][(corner+1)%sides],rings[-1][corner]])
    return model


def build_roadside(keys):
    model=Model()
    rng=random.Random(VEGETATION_SEED)
    for index in range(550):
        z=rng.uniform(-24.,235.)
        side=rng.choice((-1,1))
        x=road_at(keys,z)[0]+side*(4.8+51.*rng.random()**1.9)
        y=ground_at(keys,x,z)-.02
        scale=rng.uniform(.24,1.13)
        for branch in range(7):
            angle=rng.random()*math.tau
            radius=scale*rng.uniform(.16,.67)
            leaf=(x+radius*math.cos(angle), y+scale*rng.uniform(.39,.77), z+radius*math.sin(angle))
            model.tube("stem",(x,y,z),leaf,.010*scale,4)
            for blade in range(4):
                heading=angle+blade*1.36
                dx,dz=math.cos(heading)*scale,math.sin(heading)*scale
                tip=(leaf[0]+dx*.21,leaf[1]+.025*scale,leaf[2]+dz*.21)
                model.face("leaf",[leaf,(leaf[0]+dx*.10-dz*.044,leaf[1]+.07*scale,leaf[2]+dz*.10+dx*.044),tip])
    for distance in range(11,190,18):
        center=road_at(keys,distance)[0]
        for side in (-1,1):
            x=center+side*5.25
            y=ground_at(keys,x,distance)
            model.box("timber",x-.047,y,distance-.04,.094,.72,.08)
            model.box("line",x-.053,y+.58,distance-.048,.106,.088,.096)
    for distance in (27.,53.,79.,105.,131.,157.):
        x=road_at(keys,distance)[0]-9.4
        y=ground_at(keys,x,distance)
        model.tube("timber",(x,y,distance),(x,y+5.9,distance),.122)
        model.box("timber",x-1.12,y+5.31,distance-.073,2.24,.137,.146)
        for offset in (-.84,.84):
            model.box("metal",x+offset-.043,y+5.447,distance-.043,.086,.096,.086)
            if distance<157.:
                for section in range(8):
                    ends=[]
                    for step in (section,section+1):
                        phase=step/8.
                        z=distance+26.*phase
                        target=road_at(keys,z)[0]-9.4+offset
                        top=ground_at(keys,target-offset,z)+5.534
                        ends.append((target,top-.43*math.sin(math.pi*phase),z))
                    model.tube("metal",*ends,.017,4)
    return model


def build_desert(output):
    source = Path(__file__).resolve().parents[2]/"assets/models/desert/route.json"
    layout = json.loads(source.read_text(encoding="utf-8"))
    keys=layout["road"]
    build_ground(keys).write(output,"road_corridor",COLORS,(SOIL,ASPHALT))
    build_mesas(layout["mesas"],keys).write(output,"sandstone_mesas",MESA_COLORS,
        {band:"large_sandstone_blocks_01" for band in ("ochre","rock","red","band")})
    build_roadside(keys).write(output,"roadside",COLORS,())
    (output/"route.json").write_text(json.dumps({"road":keys}),encoding="utf-8")
