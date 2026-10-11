# One cumulative E2E-5 console test

The final `Switch-E2E5-TEST.nro` will be placed at the repository root only
after the software acceptance gates pass. There is no E2E-5 NRO ready now.

When ready, copy that single file to the Switch game directory and retain the
exact SHA-256 published with it. In one session, observe: boot and menu;
pointer motion; music with and without menu transitions; New Game, full
GameSettings first frame, several option changes and OK; generation,
Introduction and gameplay; SFX; save, reload and restart. Copy the diagnostic
log and crash report back afterward. Record each checkpoint as observed,
failed, or not reached. Existing user saves must be backed up before any save
exercise, and the experimental quicksave path must not replace the ordinary
build until its transaction recovery has passed all gates.

This is a plan, not a claim that these stages run today.
