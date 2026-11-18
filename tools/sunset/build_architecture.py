"""Bake the authored coastal building components to editable glTF meshes."""
from collections import defaultdict
import json
import math
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[2]
WOOD = "wood_planks_grey"
STONE = "large_sandstone_blocks_01"
TILE = "roof_09"
PLASTER = "coast_sand_01"
MATERIALS = {
    WOOD: (1., .95, .86, 1.),
    STONE: (.94, .88, .79, 1.),
    TILE: (1., 1., 1., 1.),
    PLASTER: (1., .95, .85, 1.),
    "frame": (.20, .115, .065, 1.),
    "glass": (.14, .29, .36, 1.),
    "iron": (.16, .14, .11, 1.),
}


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def minus(a, b):
    return tuple(left-right for left, right in zip(a, b))


class Model:
    def __init__(self):
        self.parts = defaultdict(list)

    def face(self, kind, points, uv=None):
        normal = cross(minus(points[1], points[0]), minus(points[2], points[0]))
        length = math.hypot(*normal)
        if length < 1e-7:
            return
        normal = tuple(component / length for component in normal)
        if uv is None:
            vertical = abs(normal[1]) < .70
            uv = [(z*.53 if abs(normal[0]) > .66 else x*.53, -y*.56 if vertical else z*.58)
                  for x,y,z in points]
        part = self.parts[kind]
        for index in range(1, len(points)-1):
            for vertex in (0, index, index+1):
                part.append((*points[vertex], *normal, *uv[vertex]))

    def box(self, kind, x, y, z, width, height, depth):
        right, top, far = x+width, y+height, z+depth
        self.face(kind, [(right,y,z),(x,y,z),(x,top,z),(right,top,z)])
        self.face(kind, [(x,y,far),(right,y,far),(right,top,far),(x,top,far)])
        self.face(kind, [(x,y,z),(x,y,far),(x,top,far),(x,top,z)])
        self.face(kind, [(right,y,far),(right,y,z),(right,top,z),(right,top,far)])
        self.face(kind, [(x,top,far),(right,top,far),(right,top,z),(x,top,z)])
        self.face(kind, [(x,y,z),(right,y,z),(right,y,far),(x,y,far)])

    def tube(self, kind, start, end, radius, sides=8):
        axis = minus(end, start)
        length = math.hypot(*axis)
        axis = tuple(component / length for component in axis)
        right = cross(axis, (0.,1.,0.) if abs(axis[1]) < .9 else (1.,0.,0.))
        right = tuple(component / math.hypot(*right) for component in right)
        up = cross(right, axis)
        rings = [[tuple(origin + radius*(math.cos(math.tau*k/sides)*r + math.sin(math.tau*k/sides)*u)
                        for origin,r,u in zip(center,right,up)) for k in range(sides)]
                 for center in (start,end)]
        for side in range(sides):
            following = (side+1) % sides
            self.face(kind, [rings[0][side],rings[1][side],rings[1][following],rings[0][following]])

    def write(self, root, name):
        output = root / name
        output.mkdir(parents=True, exist_ok=True)
        paths, textures, materials, primitives, views, accessors = [], [], [], [], [], []
        payload = bytearray()
        for kind, vertices in self.parts.items():
            material = {"name": kind, "pbrMetallicRoughness": {
                "baseColorFactor": MATERIALS[kind], "metallicFactor": 0., "roughnessFactor": .78}}
            if kind in (WOOD, STONE, TILE, PLASTER):
                start = len(paths)
                for channel in ("diff","nor_gl","arm"):
                    paths.append({"uri": f"../{kind}/{channel}.png"})
                    textures.append({"source": len(paths)-1})
                material["pbrMetallicRoughness"].update({"baseColorTexture": {"index": start},
                    "metallicRoughnessTexture": {"index": start+2}, "roughnessFactor": 1.})
                material["normalTexture"] = {"index": start+1, "scale": .77}
                material["occlusionTexture"] = {"index": start+2}
            if kind == "glass":
                material["pbrMetallicRoughness"]["roughnessFactor"] = .21
            materials.append(material)
            offset = len(payload)
            for vertex in vertices:
                payload.extend(struct.pack("<8f", *vertex))
            view = len(views)
            views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload)-offset,
                          "byteStride": 32, "target": 34962})
            attributes = {}
            for semantic, byte_offset, shape in (("POSITION",0,"VEC3"),("NORMAL",12,"VEC3"),("TEXCOORD_0",24,"VEC2")):
                attributes[semantic] = len(accessors)
                data = {"bufferView": view, "byteOffset": byte_offset, "componentType": 5126,
                        "count": len(vertices), "type": shape}
                if semantic == "POSITION":
                    columns = list(zip(*vertices))
                    data.update(min=[min(column) for column in columns[:3]],
                                max=[max(column) for column in columns[:3]])
                accessors.append(data)
            primitives.append({"attributes": attributes, "material": len(materials)-1, "mode": 4})
        gltf = {"asset": {"version": "2.0", "generator": "Noctis coastal model exporter"},
                "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
                "meshes": [{"primitives": primitives}], "materials": materials, "images": paths, "textures": textures,
                "bufferViews": views, "accessors": accessors, "buffers": [{"uri": name+".bin", "byteLength": len(payload)}]}
        (output / (name+".bin")).write_bytes(payload)
        (output / (name+"_1k.gltf")).write_text(json.dumps(gltf,separators=(",",":")), encoding="utf-8")
        print(f"Built {name}: {sum(len(part) for part in self.parts.values())//3} triangles")


def build_pier(spec):
    model = Model()
    for left,right,near,far,top,direction in spec["decks"]:
        start, end = (near,far) if direction == "across" else (left,right)
        count = math.ceil((end-start)/.249)
        step = (end-start)/count
        for board in range(count):
            lead = start+board*step
            lift = .0025*math.sin(board*4.8)
            if direction == "across":
                model.box(WOOD,left,top-.083+lift,lead,right-left,.083,step-.005)
            else:
                model.box(WOOD,lead,top-.083+lift,near,step-.005,.083,far-near)
        if direction == "across":
            for x in (-2.82,2.66):
                model.box(WOOD,x,-.219,near,.16,.166,far-near)
        else:
            for z in (near,far-.13):
                model.box(WOOD,left,-.222,z,right-left,.168,.13)
    for start,end,segments in spec["rails"]:
        posts = []
        for index in range(segments+1):
            u = index / segments
            x,z = [left+(right-left)*u for left,right in zip(start,end)]
            model.box(WOOD,x-.074,-1.48,z-.074,.148,2.39,.148)
            model.box(WOOD,x-.093,.91,z-.093,.186,.034,.186)
            model.box("iron",x-.079,.67,z-.079,.158,.041,.158)
            posts.append((x,z))
        for (ax,az),(bx,bz) in zip(posts,posts[1:]):
            for level in (.45,.74):
                chain = [(ax+(bx-ax)*(k/6), level-.101*math.sin(math.pi*k/6), az+(bz-az)*(k/6)) for k in range(7)]
                for first,second in zip(chain,chain[1:]):
                    model.tube("frame",first,second,.021,6)
    for x,z in spec["landing_piles"]:
        model.box(WOOD,x,-1.5,z,.28,1.29,.28)
    for z in (-11.,-7.7,-4.7,1.,5.8,11.1,15.9):
        model.tube(WOOD,(-2.62,-.82,z),(2.62,-.25,z),.087,6)
    for x in (-13.4,-10.8,-8.2,-5.9):
        model.box(WOOD,x,-1.41,-2.98,.18,1.36,.18)
    model.box(WOOD,-13.35,.031,-2.28,.49,.37,.55)
    model.tube("iron",(-13.11,.21,-2.03),(-14.19,1.62,-1.81),.032)
    model.tube("iron",(-14.19,1.62,-1.81),(-14.38,-.57,-1.83),.004,4)
    return model


def build_villa(spec):
    model = Model()
    left,right,near,far,base,top = spec["walls"]
    model.box(STONE,left,base,near,right-left,.25,far-near)
    model.box(PLASTER,left+.17,base+.25,near,right-left-.17,top-base-.25,.21)
    model.box(PLASTER,left+.17,base+.25,far-.21,right-left-.17,top-base-.25,.21)
    model.box(PLASTER,right-.19,base+.25,near,.19,top-base-.25,far-near)
    model.box(PLASTER,left,2.20,near,.20,top-2.20,far-near)
    cursor = near
    for index,center in enumerate(spec["bays"]):
        start,end = center-.48,center+.48
        model.box(PLASTER,left,.43,cursor,.20,1.77,start-cursor)
        low = .44 if index == spec["door_bay"] else .82
        if low > .44:
            model.box(PLASTER,left,.43,start,.20,low-.43,end-start)
        model.box("glass",left+.074,low,start,.025,2.20-low,end-start)
        for z in (start,center-.021,end-.043):
            model.box("frame",left-.055,low,z,.135,2.20-low,.045)
        for y in (low,1.43,2.16):
            model.box("frame",left-.059,y,start,.133,.049,end-start)
        model.box(STONE,left-.18,low-.050,start-.065,.26,.05,end-start+.13)
        cursor = end
    model.box(PLASTER,left,.43,cursor,.20,1.77,far-cursor)
    for x in (left-.13,right-.18):
        for z in (near-.095,far-.21):
            model.box(STONE,x,.21,z,.29,2.63,.29)
    model.box("frame",left-.27,2.64,near-.22,right-left+.54,.14,far-near+.44)
    xl,xr,zn,zf,eave,ridge = spec["roof"]
    mid = (xl+xr)*.5
    model.face(PLASTER,[(xr,eave,zn+.25),(xl,eave,zn+.25),(mid,ridge,zn+.25)])
    model.face(PLASTER,[(xl,eave,zf-.25),(xr,eave,zf-.25),(mid,ridge,zf-.25)])
    model.face(TILE,[(xl,eave,zf),(mid,ridge,zf),(mid,ridge,zn),(xl,eave,zn)],
               [(0,0),(0,1.1),(1.83,1.1),(1.83,0)])
    model.face(TILE,[(mid,ridge,zf),(xr,eave,zf),(xr,eave,zn),(mid,ridge,zn)],
               [(0,1.1),(0,0),(1.83,0),(1.83,1.1)])
    for index in range(21):
        z = zn+index*(zf-zn)/21
        model.tube(TILE,(mid,ridge+.035,z),(mid,ridge+.035,min(zf,z+.26)),.075,8)
    for z in (4.21,5.42,6.76,7.97):
        model.box("frame",left-.23,2.39,z-.037,.32,.085,.074)
    return model


def build_ruins(spec):
    model = Model()
    for index,(x,y,z,radius,height,lean) in enumerate(spec["columns"]):
        size = radius*3.17
        model.box(STONE,x-size/2,y-.5,z-size/2,size,.66,size)
        model.box(STONE,x-radius*1.26,y+.14,z-radius*1.26,radius*2.52,.34,radius*2.52)
        levels = max(4,round(height/1.28))
        sides = 24
        for drum in range(levels):
            ring = []
            for edge in (0,1):
                fraction = (drum+edge)/levels
                points = []
                for side in range(sides):
                    angle = side*math.tau/sides + index*.13
                    chipped = .10*math.sin(side*2.11+drum*3.6+index) + .042*math.sin(side*4.53+drum)
                    fluting = .065 if side % 2 else 0.
                    width = radius*(1.06-.21*fraction-chipped-fluting)
                    broken = (.19+.22*math.sin(angle*2.4+index)) if drum == levels-1 and edge and index not in (2,3) else 0.
                    points.append((x+lean*height*fraction+width*math.cos(angle),
                                   y+.46+height*fraction-.026*edge-broken,z+width*math.sin(angle)))
                ring.append(points)
            for side in range(sides):
                following = (side+1) % sides
                model.face(STONE,[ring[0][side],ring[1][side],ring[1][following],ring[0][following]],
                           [(side*.10,-ring[0][side][1]*.48),(side*.10,-ring[1][side][1]*.48),
                            ((side+1)*.10,-ring[1][following][1]*.48),((side+1)*.10,-ring[0][following][1]*.48)])
            if drum == levels-1:
                for side in range(sides):
                    model.face(STONE,[(x+lean*height,y+height+.39,z),ring[1][(side+1)%sides],ring[1][side]])
        if index in (2,3):
            model.box(STONE,x+lean*height-radius*1.24,y+height+.34,z-radius*1.24,
                      radius*2.48,.48,radius*2.48)
    for dimensions in spec["lintels"]:
        model.box(STONE,*dimensions)
    return model


def build_architecture(output):
    layout = json.loads((ROOT / "assets/models/coastal/architecture.json").read_text(encoding="utf-8"))
    for name,builder in (("pier",build_pier),("villa",build_villa),("ruins",build_ruins)):
        builder(layout[name]).write(output, name)
