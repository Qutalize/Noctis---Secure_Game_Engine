"""Keep the CC0 figures' original geometry and convert their textures for the runtime."""
import json
import struct
import subprocess


def convert_figure(original, directory, name):
    total, document_size, document_type = struct.unpack_from("<III", original, 8)
    binary_header = 20 + document_size
    binary_size, binary_type = struct.unpack_from("<II", original, binary_header)
    if (original[:8] != b"glTF\x02\x00\x00\x00" or total != len(original)
            or document_type != 0x4E4F534A or binary_type != 0x004E4942
            or binary_header + 8 + binary_size != total):
        raise ValueError(f"Invalid source GLB: {name}")
    document = json.loads(original[20:binary_header])
    payload = memoryview(original)[binary_header+8:]
    views = document["bufferViews"]
    image_views = set()
    for index, image in enumerate(document["images"]):
        view_index = image.pop("bufferView")
        image_views.add(view_index)
        image.pop("mimeType")
        view = views[view_index]
        start = view.get("byteOffset", 0)
        uri = f"{name}-image-{index}.png"
        subprocess.run(["ffmpeg", "-v", "error", "-nostdin", "-y", "-f", "image2pipe",
                        "-i", "pipe:0", "-frames:v", "1", "-vf",
                        "scale=512:512:force_original_aspect_ratio=decrease", "-pix_fmt", "rgba",
                        str(directory / uri)], input=bytes(payload[start:start+view["byteLength"]]), check=True)
        image["uri"] = uri
    # Repack geometry and animation buffers without keeping the large embedded source images.
    geometry = bytearray()
    view_ids = {}
    retained = []
    for index, view in enumerate(views):
        if index in image_views:
            continue
        begin = view.get("byteOffset", 0)
        view["byteOffset"] = len(geometry)
        geometry.extend(payload[begin:begin+view["byteLength"]])
        geometry.extend(b"\0" * (-len(geometry) % 4))
        view_ids[index] = len(retained)
        retained.append(view)
    for accessor in document["accessors"]:
        if "bufferView" in accessor:
            accessor["bufferView"] = view_ids[accessor["bufferView"]]
        if "sparse" in accessor:
            for entry in ("indices", "values"):
                sparse = accessor["sparse"][entry]
                sparse["bufferView"] = view_ids[sparse["bufferView"]]
    document["bufferViews"] = retained
    document["buffers"] = [{"byteLength": len(geometry), "uri": name+".bin"}]
    (directory / (name+".bin")).write_bytes(geometry)
    (directory / (name+".gltf")).write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
