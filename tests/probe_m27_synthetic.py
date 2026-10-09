#!/usr/bin/env python3
"""Small independent M27 geometry, ordering and timing reference cases."""


def inside(rect, point):
    left, top, right, bottom = rect
    x, y = point
    return left <= x < right and top <= y < bottom


def occlusion(nodes, point, ignored):
    # The visual query visits last sibling first and stops at a blocker.
    for name, rect, blocking in reversed(nodes):
        if inside(rect, point):
            if name == ignored:
                return -1
            if blocking:
                return 1
    return 0


def caret_step(elapsed, delta):
    elapsed += delta
    return (0, 1) if elapsed > 200 else (elapsed, 0)


def main():
    nodes = [("a", (5, 5, 55, 55), False),
             ("b", (10, 10, 60, 60), True)]
    assert [name for name, rect, _ in nodes if inside(rect, (20, 20))] == ["a", "b"]
    assert not inside(nodes[0][1], (55, 20))  # half-open right edge
    assert occlusion(nodes, (20, 20), "a") == 1
    assert occlusion(nodes, (20, 20), "b") == -1
    assert occlusion(nodes, (90, 90), "a") == 0
    # Graph hit uses a pixel mask after the rectangle admits both points.
    graph_bounds = (4, 4, 6, 5)
    pixels = (0, 0xffff)
    assert inside(graph_bounds, (4, 4)) and inside(graph_bounds, (5, 4))
    assert not pixels[0] and pixels[1]

    normal_down = False
    normal_down = True                # hover + valid left-down
    assert normal_down
    normal_down = False               # valid left-up
    assert not normal_down
    fixed_down = False
    fixed_down = not fixed_down       # first Fix press
    assert fixed_down
    fixed_down = not fixed_down       # second Fix press
    assert not fixed_down

    minimum, maximum, start = 1, 200, 1
    denominator = 251 - 21 - 21 - 56
    drag_anchor = start - (maximum - minimum) * (50 - 21) // denominator
    dragged = min(maximum, max(minimum,
        drag_anchor + (maximum - minimum) * (90 - 21) // denominator))
    assert denominator == 153 and dragged == 53
    assert [dragged - 1, dragged - 2, dragged - 5] == [52, 51, 48]
    # Upstream resets its last tick to now; one delayed update blinks once.
    assert caret_step(0, 200) == (200, 0)
    assert caret_step(200, 1) == (0, 1)
    assert caret_step(0, 402) == (0, 1)
    print("M27 SYNTHETIC INPUT ORACLE PASS")


if __name__ == "__main__":
    main()
