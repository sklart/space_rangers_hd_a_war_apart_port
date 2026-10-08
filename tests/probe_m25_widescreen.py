#!/usr/bin/env python3
"""Read-only 1280x720 GI fixup overlap with release UI GI references."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def fixup(key: str, dimensions: list[int] | None) -> tuple[str, str] | None:
    # At 1280x720, upstream ExtraScreenWidth=256 and ExtraScreenHeight=-48;
    # EC_CacheGI clamps only its local ExtraHeight to zero.
    if key == "Bm.PanelMain2.2BG": return "width>0", "nine-patch to 1280, relocate origin"
    if key == "Bm.FormMain2.2AnimMain": return "width>0", "two nine-patch regions to 1280x768"
    if key == "Bm.FormShop2.2bg": return "width>0, delta=198", "five nine-patch regions, +198 width"
    if key in ("Bm.FormShop2.2Fei", "Bm.FormShop2.2Gaal",
               "Bm.FormShop2.2Peleng", "Bm.FormShop2.2People"):
        return "width>0, delta=198", "horizontal nine-patch, +198 width"
    if key == "Bm.FormGameSet2.2Footer": return "width>0", "three-region footer expansion"
    if key in ("Bm.FormIntro2.PanelTop", "Bm.FormEnd2.PanelTop"):
        return "unconditional", "full-width rescale and raw GI encode"
    if key == "Bm.FormIntro2.PanelBottom":
        return "width>0", "two-region horizontal panel expansion"
    if key == "Bm.FormEnd2.PanelBottom":
        return "unconditional", "full-width rescale and raw GI encode"
    if key == "Bm.FormPQuest2.2Panel":
        return "width>0", "vertical two-region nine-patch into 1280x720"
    if key.startswith("Bm.FormPQuest2.2S") and key[18:].isdigit() or \
       key.startswith("Bm.FormPQuest2.") and "rescale" in key:
        return "width>0", "two-region quest picture rescale"
    if key in ("Bm.FormRuins.2WBbg", "Bm.FormRuins.2CBbg",
               "Bm.FormRuins.2DestroyerBridgebg"):
        return "unconditional", "full-screen RGBA rescale"
    if key in ("Bm.FormAbout2.Bg", "Bm.FormGameSet2.2bg",
               "Bm.FormOptions2.2Bg", "Bm.FormScore2.2bg") or key.startswith("Bm.City."):
        return "fallback", "full-screen RGBA rescale"
    if key.startswith("Bm.Gov.") and any(token in key for token in
            ("MalocBG", "MalocPirateBG", "PelengBG", "PelengPirateBG", "PeopleBG",
             "PeoplePirateBG", "FeiBG", "FeiPirateBG", "GaalBG", "GaalPirateBG", "PirateBG")):
        return "fallback", "full-screen RGBA rescale"
    if key.startswith("Bm.FormRuins.") and ("BKbg" in key or any(token in key for token in
            ("MCbg", "PBbg", "RCbg", "SBbg", "WBbg2")) or
            ("bg" in key and "table" not in key)):
        return "fallback", "full-screen RGBA rescale, ruin alignment"
    if key.startswith("Bm.PlanetBG") and dimensions and dimensions[0] <= 1024 and dimensions[1] <= 768:
        return "fallback", "full-screen RGBA rescale"
    if key.startswith("Bm.FormLoad2.Shutter"):
        return "fallback", "full-screen RGBA rescale, preserve alpha"
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("release_inventory", type=Path)
    args = parser.parse_args()
    source = json.loads(args.release_inventory.read_text(encoding="utf-8-sig"))
    rows = []
    for key, resource in source["gi"]["resources"].items():
        match = fixup(key, resource.get("dimensions"))
        if match:
            rows.append({"resource": key, "condition_1280x720": match[0],
                         "transformation": match[1], "status": resource["status"],
                         "references": resource["references"]})
    print(json.dumps({"screen": [1280, 720], "extra_width": 256, "extra_height": -48,
                      "selected_subtree_fixups": [row for row in rows if row["resource"] in
                          ("Bm.FormLoad2.2BarLeft", "Bm.FormLoad2.2BarCenter", "Bm.FormLoad2.2BarRight")],
                      "matched_release_keys": len(rows), "resources": rows}, ensure_ascii=False, indent=2))
