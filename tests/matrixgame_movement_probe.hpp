// Test observer/command owner; the game performs all navigation and movement.
#pragma once
struct SObjectCore;
class MovementProbe {
public:
 ~MovementProbe();
 void Issue();void Verify();
 MovementProbe()=default;MovementProbe(const MovementProbe&)=delete;
 MovementProbe& operator=(const MovementProbe&)=delete;
private:
 SObjectCore* core_{};int x_{},y_{},target_x_{},target_y_{},time_{};
 float world_x_{},world_y_{};
};
