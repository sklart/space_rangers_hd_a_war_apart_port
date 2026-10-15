"""Keep the original ship equipment cache anchored to native-width members."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from e2e_source_overrides import generated_source  # noqa: E402


class ShipSlotOverrideTests(unittest.TestCase):
    def test_generated_ship_cache_uses_actual_hull_member(self) -> None:
        source = ROOT / "upstream/cpp/src/aShip.cpp"
        with tempfile.TemporaryDirectory() as folder:
            output = generated_source(source, Path(folder) / "aShip.cpp")
            text = output.read_text(encoding="utf-8")
        self.assertNotIn("reinterpret_cast<PShipEquipmentCacheView>", text)
        self.assertEqual(text.count("pas::byte_offset(&Self->Hull"), 3)
        self.assertEqual(text.count("pas::byte_offset(&Hull"), 12)
        self.assertIn("offsetof(aShip::TShip, DefGenerator)", text)

    def test_generated_ranger_rating_keeps_full_pointer_width(self) -> None:
        source = ROOT / "upstream/cpp/src/aGalaxy.cpp"
        with tempfile.TemporaryDirectory() as folder:
            output = generated_source(source, Path(folder) / "aGalaxy.cpp")
            text = output.read_text(encoding="utf-8")
        start = text.index("void TGalaxy::RefreshRangerRatingPlaces()")
        end = text.index("\n    }", start)
        rating = text[start:end]
        self.assertIn("pas::DynArray<std::uintptr_t> Sorted", rating)
        self.assertIn("std::uintptr_t SwapFirst", rating)
        self.assertNotIn("static_cast<std::uint32_t>", rating)

    def test_encoded_player_keeps_full_pointer_width(self) -> None:
        source = ROOT / "upstream/cpp/src/aPlayer.cpp"
        with tempfile.TemporaryDirectory() as folder:
            output = generated_source(source, Path(folder) / "aPlayer.cpp",
                                      script_width=True)
            text = output.read_text(encoding="utf-8")
        self.assertIn("std::uintptr_t EncodedPlayer", text)
        self.assertNotIn("static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Player))", text)
        self.assertIn("EncodedPlayer ^ 0xb1cd15d3u", text)
        self.assertNotIn("reinterpret_cast<aShip::PShipEquipmentCacheView>(this)->Slots", text)
        self.assertIn("pas::byte_offset(&Hull, (Kind - 42)", text)

    def test_script_order_targets_keep_full_pointer_width(self) -> None:
        source = ROOT / "upstream/cpp/src/aShip.cpp"
        with tempfile.TemporaryDirectory() as folder:
            output = generated_source(source, Path(folder) / "aShip.cpp")
            text = output.read_text(encoding="utf-8")
        self.assertEqual(text.count("static_cast<std::uintptr_t>(State->TargetValue)"), 7)
        self.assertEqual(text.count("static_cast<std::uintptr_t>(Place->TargetValue)"), 3)
        self.assertNotIn("static_cast<std::uint32_t>(State->TargetValue)", text)
        self.assertNotIn("static_cast<std::uint32_t>(Place->TargetValue)", text)

    def test_pirate_service_planet_queue_uses_live_list(self) -> None:
        source = ROOT / "upstream/cpp/src/aPirate.cpp"
        with tempfile.TemporaryDirectory() as folder:
            output = generated_source(source, Path(folder) / "aPirate.cpp")
            text = output.read_text(encoding="utf-8")
        start = text.index("aPlanet::TPlanet* TPirate::SelectServicePlanet()")
        end = text.index("void TPirate::BuildReachablePlanetQueue()", start)
        selection = text[start:end]
        self.assertIn("pas::list_at<aPlanet::TPlanet>(PlanetQueue, 0)", selection)
        self.assertNotIn("static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(PlanetQueue))", selection)


if __name__ == "__main__":
    unittest.main()
