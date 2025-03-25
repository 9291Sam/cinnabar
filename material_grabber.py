import os
import json

out = os.popen("curl --request GET --url https://api.physicallybased.info/materials --header 'Accept: application/json'").read()
jsonData = json.loads(out)

cpp_code = """
switch (v)
{
"""

for d in jsonData:
    name = d.get("name")
    voxel_name = f"Voxel::{name}"
    
    # Ensure float values always have a decimal point and 'f' suffix
    color = [f"{c}f" if "." in str(c) else f"{c}.0f" for c in d.get("color", [0.0, 0.0, 0.0])]
    metallic = f"{d.get('metalness', 0.0)}f" if "." in str(d.get("metalness", 0.0)) else f"{d.get('metalness', 0.0)}.0f"
    roughness = f"{d.get('roughness', 1.0)}f" if "." in str(d.get("roughness", 1.0)) else f"{d.get('roughness', 1.0)}.0f"
    emissive = [f"{e}f" if "." in str(e) else f"{e}.0f" for e in d.get("emissiveColor", [0.0, 0.0, 0.0])]

    cpp_code += f"""
    case {voxel_name}:
        return PBRVoxelMaterial {{
            .albedo_roughness {{{color[0]}, {color[1]}, {color[2]}, {roughness}}},
            .emission_metallic {{{emissive[0]}, {emissive[1]}, {emissive[2]}, {metallic}}},
        }};
        break;
    """

cpp_code += "\n    default: return PBRVoxelMaterial{};\n}"

print(cpp_code)
