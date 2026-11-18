"""Build the continuous meadow, rain forest and industrial approach."""
import math
import random

from build_architecture import Model
from build_desert import road_at


FOREST_START, FOREST_END = 267., 617.
GROUND_COLORS = {"meadow": (1., 1., .89, 1.), "trail": (1., .93, .84, 1.),
                 "forest": (.82, .89, .78, 1.), "yard": (.70, .72, .73, 1.)}
GROUND_MAPS = {"meadow": "leafy_grass", "trail": "rocky_trail",
               "forest": "forest_ground_04", "yard": "mud_forest"}
TREE_COLORS = {"bark": (.76, .75, .72, 1.), "needles": (.62, .84, .67, 1.),
               "young": (.76, .93, .69, 1.), "grass": (.84, 1., .69, 1.)}
TREE_MAPS = {"bark": "pine_bark", "needles": "pine_sapling_medium",
             "young": "pine_sapling_medium", "grass": "grass_medium_01"}


def height(layout, x, z):
    center, axis_height = road_at(layout["road"], z)
    distance = abs(x-center)
    freedom = min(1., max(0., (distance-2.6)/20.))
    ridges = sum(amplitude * math.exp(-((x-a)/width)**2-((z-b)/length)**2)
                 for a,b,width,length,amplitude in layout["hills"])
    folds = (4.8*math.sin(x*.041+z*.014) + 2.6*math.cos(x*.087-z*.039)
             + .6*math.sin(x*.24+z*.14)) * (1. if z<615. else .3)
    industrial = min(1., max(0., (z-594.)/38.)) * min(1., max(0., (980.-z)/80.))
    outside_yard = min(1., max(0., (distance-50.)/54.))
    return axis_height - .025 + freedom*(ridges+folds)*(1.-industrial*(1.-outside_yard))


def normal(layout, x, z):
    dx = height(layout,x-.5,z)-height(layout,x+.5,z)
    dz = height(layout,x,z-.5)-height(layout,x,z+.5)
    size = math.hypot(dx,1.,dz)
    return dx/size,1./size,dz/size


def terrain(layout):
    model = Model()
    offsets = [-700,-570,-450,-335,-249,-189,-144,-110,-85,-66,-52,-43,-35,-28,-22,-17,-13,
               -10,-8,-6,-4.6,-3.5,-2.7,-1.1,0,1.1,2.7,3.5,4.6,6,8,10,13,17,22,28,35,
               43,52,66,85,110,144,189,249,335,450,570,700]
    steps = sorted(set(range(-90,1361,5)) | {k[0] for k in layout["road"]} | {267,617,954})
    rows = {}
    for z in steps:
        center,_ = road_at(layout["road"],z)
        half_width = (.52*math.sin(z*.065)+.27*math.sin(z*.23)) if z<617. else .25
        row = []
        for offset in offsets:
            x = center+offset+(math.copysign(half_width,offset) if abs(offset)==2.7 else 0.)
            row.append(((x,height(layout,x,z),z),normal(layout,x,z)))
        rows[z] = row
    for z0,z1 in zip(steps,steps[1:]):
        for column in range(len(offsets)-1):
            if abs(offsets[column]+offsets[column+1]) < 4. and -42<=z0<954:
                material = "trail" if z0<617 else "yard"
            elif FOREST_START<=z0<FOREST_END:
                material = "forest"
            elif 617<=z0<954:
                material = "yard"
            else:
                material = "meadow"
            points,normals = zip(rows[z0][column], rows[z1][column], rows[z1][column+1], rows[z0][column+1])
            model.face(material,points,[(x*.49,z*.49) for x,_,z in points],normals)
    return model


def card(model, material, center, length, width, angle, incline, upright=False):
    x,y,z = center
    dx,dz = math.cos(angle),math.sin(angle)
    rx,rz = -dz*width*.5,dx*width*.5
    if upright:
        points = [(x-rx,y,z-rz),(x+rx,y,z+rz),
                  (x+rx+dx*incline,y+length,z+rz+dz*incline),(x-rx+dx*incline,y+length,z-rz+dz*incline)]
    else:
        points = [(x-rx,y,z-rz),(x+rx,y,z+rz),
                  (x+rx+dx*length,y+incline,z+rz+dz*length),(x-rx+dx*length,y+incline,z-rz+dz*length)]
    model.face(material,points,[(.015,.99),(.985,.99),(.985,.01),(.015,.01)])


def vegetation(layout):
    grass, wood, leaves = Model(), Model(), Model()
    rng = random.Random(528813)
    for index in range(11300):
        z = rng.uniform(-34.,257.)
        center,_ = road_at(layout["road"],z)
        spread = 36. if index<8100 else 155.
        x = center+rng.choice((-1,1))*rng.uniform(3.,spread)
        clump = math.sin(x*.27+z*.14)*math.sin(z*.17-x*.11)
        if clump<-.28 and rng.random()<.67:
            continue
        y = height(layout,x,z)-.04
        size = rng.uniform(.17,.49)*(1.+.27*clump)
        angle = rng.uniform(0.,math.tau)
        for rotation in (angle,angle+1.42):
            card(grass,"grass",(x,y,z),size,size*1.65,rotation,size*.23,True)
    for index in range(760):
        z = rng.uniform(239.,621.)
        center,_ = road_at(layout["road"],z)
        offset = rng.choice((-1,1))*rng.uniform(5.7,45. if index<430 else 177.)
        x = center+offset
        if abs(z-398.)<12. and abs(offset)<14.:
            continue
        y = height(layout,x,z)-.20
        mature = rng.uniform(10.,27.) if abs(offset)>15. else rng.uniform(9.,21.)
        lean_x,lean_z = rng.uniform(-.48,.48),rng.uniform(-.48,.48)
        half = (x+lean_x,y+mature*.57,z+lean_z)
        peak = (x+lean_x*1.4,y+mature,z+lean_z*1.4)
        wood.tube("bark",(x,y,z),half,mature*.016,8)
        wood.tube("bark",half,peak,mature*.008,6)
        phase = rng.uniform(0.,math.tau)
        material = "young" if index%7==0 else "needles"
        for layer in range(5):
            branch_y = y+mature*(.30+layer*.141)+rng.uniform(-.4,.4)
            size = mature*(.27-layer*.041)*rng.uniform(.84,1.18)
            for sector in range(5):
                angle = phase+sector*math.tau/5+layer*2.4+rng.uniform(-.24,.24)
                start = (x+lean_x*.5,branch_y,z+lean_z*.5)
                if abs(offset)<48. and layer<3:
                    tip = (start[0]+math.cos(angle)*size*.7,branch_y+size*.10,start[2]+math.sin(angle)*size*.7)
                    wood.tube("bark",start,tip,.045,4)
                card(leaves,material,start,size,size*.70,angle,rng.uniform(-.29,.12)*size)
                if abs(offset)<45.:
                    edge = (x+math.cos(angle)*size*.35,branch_y-.17,z+math.sin(angle)*size*.35)
                    card(leaves,material,edge,size*.64,size*.48,angle+.25,size*.34,True)
    for index in range(870):
        z = rng.uniform(276.,609.)
        center,_ = road_at(layout["road"],z)
        x = center+rng.choice((-1,1))*rng.uniform(3.3,27.)
        root = (x,height(layout,x,z)-.06,z)
        size = rng.uniform(.31,.77)
        for rotation in (0.,1.3,2.84):
            card(leaves,"young",root,size,size*.96,index*.75+rotation,size*.36,True)
    return grass,wood,leaves


def build_landscape(output, layout):
    terrain(layout).write(output,"inland_ground",GROUND_COLORS,GROUND_MAPS)
    grass,wood,leaves = vegetation(layout)
    for name,model in (("inland_grasses",grass),("inland_trees",wood),("inland_canopy",leaves)):
        model.write(output,name,TREE_COLORS,TREE_MAPS)
