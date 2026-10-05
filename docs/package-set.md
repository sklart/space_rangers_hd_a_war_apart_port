# Release package set

Observed in the Windows baseline rooted at `DATA/`; package contents are not
stored in this repository. `common.pkg` is the first package used by the
Milestone 4 resource slice.

| package | bytes | startup status |
| --- | ---: | --- |
| common.pkg | 307754405 | required: current TFileEC proof asset |
| background.pkg, forms.pkg, mainmenu.pkg | 74692926 / 189030655 / 207068233 | likely UI/startup resources; not loaded yet |
| items.pkg, locations.pkg, ships.pkg, robots.pkg | 72559655 / 484864877 / 175612589 / 232546988 | gameplay resources; deferred |
| english.pkg, russian.pkg | 785801 / 885662 | language configuration dependent |
| music.pkg, Sound.pkg, voices*.pkg | 326493360 / 48553456 / 4782704+ | audio path; deferred |
| scripts.pkg, arcade.pkg, PQI.pkg, quests*.pkg, WSE.pkg | present | configuration/gameplay dependent; deferred |

The baseline also contains optional mod/tweak packages under `Mods/`. They are
not loaded by the current slice. Release ordering is recovered in
`aPacket::LoadConfiguredPackages`: loose package first, then language-mod,
language, mod, and base configuration order. Collection lookup stops at the
first matching package. The Switch adapter accepts an explicit ordered
read-only list through `OpenPackages`; current NRO proof uses only `common.pkg`.
It has no fast-name index and no loose-file write path.
