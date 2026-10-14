"""Prepare separately generated shop PNGs and a size-comparison contact sheet.

Requires Pillow. Reads exact source paths from art/ui/tomes/prompts.json;
only crops transparent padding and resizes, preserving generated artwork/alpha.
"""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/ui/shop'
OUT = ROOT / 'assets/ui/shop'
OUT.mkdir(parents=True, exist_ok=True)
manifest = json.loads((ART / 'prompts.json').read_text())
font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 17)
sheet = Image.new('RGBA', (1320, 2 * 310), (34, 43, 46, 255))
draw = ImageDraw.Draw(sheet)
for index, entry in enumerate(manifest['entries']):
    source = Image.open(entry['source']).convert('RGBA')
    alpha = source.getchannel('A')
    assert alpha.getextrema() == (0, 255), entry['name'] + ': real transparency required'
    content = alpha.getbbox()
    assert content is not None
    icon = source.crop(content)
    icon.thumbnail((216, 216), Image.Resampling.LANCZOS)
    target = Image.new('RGBA', (256, 256))
    target.alpha_composite(icon, ((256-icon.width)//2, (256-icon.height)//2))
    target.save(OUT / (entry['name'] + '.png'))
    assert target.getpixel((0,0))[3] == 0
    assert target.getpixel((255,255))[3] == 0
    x, y = (index % 5) * 264, (index // 5) * 310
    sheet.alpha_composite(target, (x+4, y))
    small = target.resize((48,48), Image.Resampling.LANCZOS)
    sheet.alpha_composite(small, (x+108, y+255))
    draw.text((x+10, y+289), entry['name'].upper(), fill=(255,243,214), font=font)
    entry['asset'] = 'assets/ui/shop/' + entry['name'] + '.png'
    entry['size'] = [256,256]
manifest['preparation'] = 'Preserved alpha; transparent-padding crop; aspect-ratio-preserving Lanczos resize into centered 216x216 content area on 256x256 RGBA canvas.'
(ART / 'prompts.json').write_text(json.dumps(manifest, indent=2)+'\n')
sheet.save(ART / 'contact-sheet.png')
print('Prepared', len(manifest['entries']), 'individual transparent shop icons.')
