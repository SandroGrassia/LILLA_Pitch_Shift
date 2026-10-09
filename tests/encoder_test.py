from pathlib import Path
import re

source = (Path(__file__).resolve().parents[1] / "src" / "HardwareControls.cpp").read_text()
table = source.split("constexpr Transition transitions[4][8] =", 1)[1].split(";", 1)[0]
cells = [(int(state), int(rotation)) for state, rotation in re.findall(r"\{(\d+), (-?\d+)\}", table)]
assert len(cells) == 32


def decode(sequence):
    state = 0
    steps = 0
    for value in sequence:
        state, rotation = cells[value * 8 + state]
        steps += rotation
    return steps


clockwise = [3, 2, 0, 1, 3]
counterclockwise = [3, 1, 0, 2, 3]
assert decode(clockwise) == -1
assert decode(counterclockwise) == 1
assert decode(clockwise * 100) == -100
assert decode(counterclockwise * 100) == 100
assert decode(clockwise + counterclockwise) == 0
assert decode([value for value in clockwise for _ in range(5)]) == -1
assert decode([value for value in counterclockwise for _ in range(5)]) == 1
assert decode([3, 2, 3]) == 0
assert decode([3, 1, 3]) == 0
assert decode([3, 0, 3]) == 0
assert decode([3, 2, 3, 2, 0, 1, 0, 1, 3]) == -1
assert decode([3, 1, 3, 1, 0, 2, 0, 2, 3]) == 1
print("PASS: both encoder directions, repeated polls, reversals, contact bounce, incomplete and invalid cycles.")
