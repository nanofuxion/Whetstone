"""JITProbe icon: teal tile + white play triangle (sibling of Whetstone)."""
from PIL import Image, ImageDraw

S = 1024
bg_top = (23, 122, 110)
bg_bot = (10, 62, 58)
play = (255, 255, 255)

img = Image.new("RGB", (S, S))
px = img.load()
for y in range(S):
    t = y / (S - 1)
    c = tuple(int(bg_top[i] + (bg_bot[i] - bg_top[i]) * t) for i in range(3))
    for x in range(S):
        px[x, y] = c

d = ImageDraw.Draw(img)
# play triangle, centered
cx, cy, r = S // 2, S // 2, 300
d.polygon([(cx - r // 2, cy - r), (cx - r // 2, cy + r),
           (cx + r, cy)], fill=play)

for size in (20, 29, 40, 58, 60, 76, 80, 87, 120, 152, 167, 180, 1024):
    img.resize((size, size), Image.LANCZOS).save(f"Icon-{size}.png")
print("wrote icons")
