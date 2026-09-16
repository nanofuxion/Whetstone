"""Whetstone icon: a sharpening stone + honing spark on dark slate."""
from PIL import Image, ImageDraw

S = 1024
bg_top = (43, 47, 54)
bg_bot = (20, 22, 26)
stone_top = (168, 173, 181)
stone_bot = (104, 110, 118)
stone_edge = (70, 75, 83)
spark = (255, 158, 44)
spark_hot = (255, 214, 130)

img = Image.new("RGB", (S, S))
px = img.load()
for y in range(S):
    t = y / (S - 1)
    px_line = tuple(int(bg_top[i] + (bg_bot[i] - bg_top[i]) * t) for i in range(3))
    for x in range(S):
        px[x, y] = px_line

# Stone block on its own layer so it can sit at a honing angle.
layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(layer)
x0, y0, x1, y1 = 150, 400, 874, 624
d.rounded_rectangle([x0, y0, x1, y1], radius=60, fill=stone_top + (255,))
# bottom bevel shading
d.rounded_rectangle([x0, y0 + 130, x1, y1], radius=60, fill=stone_bot + (255,))
d.rounded_rectangle([x0, y0, x1, y1], radius=60, outline=stone_edge + (255,), width=10)
# honing groove: thin dark line along the stone
d.line([x0 + 60, y0 + 112, x1 - 60, y0 + 112], fill=stone_edge + (255,), width=8)
layer = layer.rotate(18, resample=Image.BICUBIC, center=(S // 2, S // 2))
img = Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB")

d = ImageDraw.Draw(img)

def star(cx, cy, r, color):
    # 4-point sparkle: two thin triangles.
    d.polygon([(cx, cy - r), (cx + r // 5, cy - r // 5),
               (cx + r, cy), (cx + r // 5, cy + r // 5),
               (cx, cy + r), (cx - r // 5, cy + r // 5),
               (cx - r, cy), (cx - r // 5, cy - r // 5)], fill=color)

star(700, 300, 150, spark)
star(700, 300, 70, spark_hot)
star(300, 720, 60, spark)
star(850, 700, 40, spark_hot)

for size in (20, 29, 40, 58, 60, 76, 80, 87, 120, 152, 167, 180, 1024):
    img.resize((size, size), Image.LANCZOS).save(f"Icon-{size}.png")
print("wrote icons")
