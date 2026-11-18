"""Export the inland industrial district, forest shelter and sky as glTF."""
import json
import math
from pathlib import Path

from build_architecture import Model
from build_landscape import build_landscape, height


COLORS = {
    "rust": (.90, .73, .61, 1.), "sheet": (.70, .78, .80, 1.),
    "concrete": (.51, .49, .45, 1.), "metal": (.30, .34, .35, 1.),
    "glass": (.20, .25, .27, 1.), "tread": (.59, .31, .09, 1.),
    "lamp": (1., .67, .30, 1.), "roadbed": (.21, .23, .23, 1.),
    "wood": (.66, .58, .47, 1.), "sky": (1., 1., 1., 1.),
}
TEXTURES = {"rust": "rusty_corrugated_iron", "sheet": "corrugated_iron", "wood": "pine_bark"}


def factory(layout):
    model, lamps = Model(), Model()
    model.box("roadbed",-5.4,.923,117.,11.6,.085,63.)
    for z in range(119,178,4):
        for x in (-2.9,3.5):
            model.box("tread",x,1.009,z,.09,.013,1.3)
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


def expand_yard(model, lamps):
    for x,z,w,d,h in [(-43.,715.,31.,36.,11.), (20.,726.,33.,46.,13.),
                     (-11.,783.,24.,47.,16.), (47.,799.,21.,39.,9.)]:
        model.box("concrete",x-.5,.56,z-.5,w+1.,.59,d+1.)
        model.box("sheet",x,1.,z,w,h,d)
        model.box("rust",x-.48,1.+h,z-.48,w+.96,.8,d+.96)
        model.box("metal",x+w*.22,1.1,z-.095,w*.56,h*.47,.24)
        for side in (.11,.86):
            model.box("glass",x+w*side,h*.65,z-.08,2.2,1.13,.14)
    for z in (733.,757.,777.,819.):
        for x in (-8.,18.):
            model.tube("metal",(x,1.,z),(x,12.5,z),.34,8)
        model.tube("rust",(-8.,11.9,z),(18.,11.9,z),.32,10)
        for x in (-8.,18.):
            lamps.box("lamp",x-.15,8.5,z-.15,.29,.17,.29)
    for x,z,r,h in [(-28.,835.,2.1,43.),(-39.,843.,2.2,51.), (67.,858.,2.6,47.)]:
        model.tube("rust",(x,1.,z),(x,h,z),r,24)
        for section in (.25,.49,.72,.89):
            model.tube("sheet",(x,h*section,z),(x,h*section+.8,z),r+.08,20)
        lamps.box("lamp",x-.15,h,z-.15,.30,.18,.30)


def forest_shelter(layout):
    model = Model()
    x,z = 15.5,396.
    y = height(layout,x,z)
    model.box("concrete",x-.25,y-.17,z-.24,4.55,.31,3.65)
    for dx in (0.,3.95):
        for dz in (0.,3.04):
            model.box("wood",x+dx,y,z+dz,.16,2.73,.16)
    model.box("wood",x,y+.15,z+2.95,4.11,2.45,.15)
    model.box("wood",x+3.99,y+.15,z+.35,.14,2.45,2.59)
    model.face("rust",[(x-.39,y+2.68,z-.42),(x-.39,y+3.18,z+3.52),
                       (x+4.51,y+3.18,z+3.52),(x+4.51,y+2.68,z-.42)])
    model.box("wood",x+.46,y+.48,z+2.22,2.62,.13,.46)
    for dx in (.55,2.93):
        model.box("wood",x+dx,y+.12,z+2.34,.16,.38,.20)
    return model


def build_inland(root):
    layout = json.loads(Path(__file__).with_name("inland-layout.json").read_text(encoding="utf-8"))
    build_landscape(root,layout)
    sector, sector_lamps = factory(layout)
    yard, lamps = Model(), Model()
    for component,output in ((sector,yard),(sector_lamps,lamps)):
        for kind,vertices in component.parts.items():
            output.parts[kind].extend((x,y,z+520.,nx,ny,nz,u,v) for x,y,z,nx,ny,nz,u,v in vertices)
    expand_yard(yard,lamps)
    sky = Model()
    sky.box("sky",-440.,-440.,-440.,880.,880.,880.)
    for name,model in (("inland_yard",yard),("inland_lamps",lamps),
                       ("inland_shelter",forest_shelter(layout)),("inland_sky",sky)):
        model.write(root,name,COLORS,TEXTURES)
    (root/"route.json").write_text(json.dumps({"road": layout["road"]}),encoding="utf-8")
