"""Build the continuous meadow, rain forest and industrial approach."""
import json
import math
import random
import struct

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
    past_trail = max(0., (z-959.)/46., (-47.-z)/31.)
    freedom = min(1., max(past_trail, (distance-2.6)/20., 0.))
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
    meadow, other = Model(), Model()
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
            model = meadow if material == "meadow" else other
            scale = .43 if material == "meadow" else .49
            model.face(material,points,[(x*scale,z*scale) for x,_,z in points],normals)
    return meadow,other


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
    # The twig atlas leaves other texture islands outside the photographed branch.
    branch_uv = [(.054,.408),(.188,.408),(.210,.039),(.046,.039)]
    model.face(material,points,branch_uv)


def grass_source(directory):
    base = "grass_medium_01"
    doc = json.loads((directory / (base+"_1k.gltf")).read_text(encoding="utf-8"))
    payload = (directory / (base+".bin")).read_bytes()

    def read(index):
        accessor = doc["accessors"][index]
        view = doc["bufferViews"][accessor["bufferView"]]
        kind = {5123:"H", 5125:"I", 5126:"f"}[accessor["componentType"]]
        width = {"SCALAR":1, "VEC2":2, "VEC3":3}[accessor["type"]]
        entry = struct.Struct("<"+kind*width)
        begin = view.get("byteOffset",0) + accessor.get("byteOffset",0)
        stride = view.get("byteStride",entry.size)
        return [entry.unpack_from(payload,begin+stride*row) for row in range(accessor["count"])]

    def load(mesh_index):
        output = []
        for primitive in doc["meshes"][mesh_index]["primitives"]:
            attributes = primitive["attributes"]
            pos, normal, uv = (read(attributes[key]) for key in ("POSITION","NORMAL","TEXCOORD_0"))
            output.extend((*pos[index],*normal[index],*uv[index]) for index, in read(primitive["indices"]))
        return output

    return [load(index) for index in (3,4,5,11,12,13,14,15,16)]


def grassland(layout, variants):
    grass = Model()
    rng = random.Random(528813)
    vertices = grass.parts["grass"]
    for index in range(7600):
        tall = index < 440
        z = rng.uniform(-15.,79.) if tall else rng.uniform(-34.,257.)
        center,_ = road_at(layout["road"],z)
        spread = 10. if tall else (37. if index<5400 else 118.)
        x = center+rng.choice((-1,1))*rng.uniform(3.1,spread)
        clump = math.sin(x*.27+z*.14)*math.sin(z*.17-x*.11)
        if clump<-.28 and rng.random()<.67:
            continue
        angle = rng.uniform(0.,math.tau)
        sin,cos = math.sin(angle),math.cos(angle)
        size = rng.uniform(.82,1.58) if tall else rng.uniform(2.,3.8)
        y = height(layout,x,z)-.018
        variant = rng.choice(variants[:3] if tall else variants[3:])
        vertices.extend((x+size*(vx*cos+vz*sin),y+size*vy,z+size*(vz*cos-vx*sin),
                         nx*cos+nz*sin,ny,nz*cos-nx*sin,u,v) for vx,vy,vz,nx,ny,nz,u,v in variant)
    return grass


def forest(layout):
    wood, leaves = Model(), Model()
    rng = random.Random(110423)
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
        for branch in range(5):
            card(leaves,material,(half[0],y+mature*.83,half[2]),mature*.19,mature*.125,
                 phase+branch*math.tau/5,mature*.09,True)
    for index in range(870):
        z = rng.uniform(276.,609.)
        center,_ = road_at(layout["road"],z)
        x = center+rng.choice((-1,1))*rng.uniform(3.3,27.)
        root = (x,height(layout,x,z)-.06,z)
        size = rng.uniform(.31,.77)
        for rotation in (0.,1.3,2.84):
            card(leaves,"young",root,size,size*.96,index*.75+rotation,size*.36,True)
    return wood,leaves


def build_landscape(output, layout):
    meadow, paths = terrain(layout)
    meadow.write(output,"inland_meadow",GROUND_COLORS,GROUND_MAPS)
    paths.write(output,"inland_ground",GROUND_COLORS,GROUND_MAPS)
    source = output/"grass_medium_01"
    grass = grassland(layout,grass_source(source))
    for filename in ("grass_medium_01.bin","grass_medium_01_1k.gltf"):
        (source/filename).unlink()
    wood,leaves = forest(layout)
    for name,model in (("inland_grasses",grass),("inland_trees",wood),("inland_canopy",leaves)):
        model.write(output,name,TREE_COLORS,TREE_MAPS)
