"""Compose already-rendered 3D review views; no generated or painted replacement artwork."""
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art/models/chest_relics/refined'
SLUGS=['acorn','crystal_pan','repair_nail','lumber_boot','abacus','weapon_token','idle_spring','repair_magnet','siren','clover','mining_hammer','dawn_coin','ghost_candle','core_fuse','crystal_safe','rebuild_blueprint','lucky_die','upgrade_mirror']
LABELS=['Жёлудь','Лоток старателя','Ремонтный гвоздь','Ботинок лесоруба','Счёты','Боевой жетон','Боевая пружина','Ремонтный магнит','Сирена','Клевер','Горный молоток','Рассветная монета','Последний огонёк','Предохранитель Ядра','Кристальный сейф','Чертёж восстановления','Удачливый кубик','Зеркало улучшений']
BG=(23,32,39);CREAM=(246,237,210)
fontpath='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
font=ImageFont.truetype(fontpath,20);small=ImageFont.truetype(fontpath,17);title=ImageFont.truetype(fontpath,34)
def paste(canvas,path,pos,size):
 icon=Image.open(path).convert('RGBA');icon.thumbnail(size,Image.Resampling.LANCZOS)
 canvas.paste(icon,(pos[0]+(size[0]-icon.width)//2,pos[1]+(size[1]-icon.height)//2),icon)
def color(i):return (154,183,193) if i<5 else (92,174,240) if i<10 else (191,119,244) if i<13 else (242,180,52)
sheet=Image.new('RGB',(1800,1090),BG);d=ImageDraw.Draw(sheet)
d.text((900,35),'ПРЕДМЕТЫ — ВТОРОЙ ПРОХОД',font=title,anchor='mm',fill=CREAM)
for i,(slug,label) in enumerate(zip(SLUGS,LABELS)):
 x=(i%6)*300;y=80+(i//6)*330
 paste(sheet,ART/'views'/(slug+'_perspective.png'),(x+5,y),(290,278))
 d.line((x+42,y+281,x+258,y+281),fill=color(i),width=3)
 d.text((x+150,y+306),label,font=small,anchor='mm',fill=CREAM)
sheet.save(ART/'contact_sheet.png')
compare=Image.new('RGB',(1800,2020),BG);d=ImageDraw.Draw(compare)
d.text((900,36),'БЫЛО → СТАЛО · ОДИНАКОВЫЙ СВЕТ И МАСШТАБ',font=title,anchor='mm',fill=CREAM)
for i,(slug,label) in enumerate(zip(SLUGS,LABELS)):
 x=(i%3)*600;y=85+(i//3)*320
 d.text((x+300,y+5),label,font=font,anchor='mm',fill=CREAM)
 paste(compare,ART/'before'/(slug+'.png'),(x+8,y+25),(280,246))
 paste(compare,ART/'views'/(slug+'_perspective.png'),(x+312,y+25),(280,246))
 d.text((x+150,y+284),'БЫЛО',font=small,anchor='mm',fill=(132,152,163));d.text((x+450,y+284),'СТАЛО',font=small,anchor='mm',fill=color(i))
 d.line((x+40,y+308,x+560,y+308),fill=(50,66,76),width=1)
compare.save(ART/'before_after.png')
# Each item also has one compact five-view plate for individual review.
for slug,label in zip(SLUGS,LABELS):
 canvas=Image.new('RGB',(1600,390),BG);d=ImageDraw.Draw(canvas)
 d.text((800,25),label,font=font,anchor='mm',fill=CREAM)
 for i,(view,caption) in enumerate([('front','СПЕРЕДИ'),('side','СБОКУ'),('back','СЗАДИ'),('top','СВЕРХУ'),('perspective','ПЕРСПЕКТИВА')]):
  paste(canvas,ART/'views'/(slug+'_'+view+'.png'),(i*320+10,50),(300,280));d.text((i*320+160,355),caption,font=small,anchor='mm',fill=(175,191,197))
 canvas.save(ART/'views'/(slug+'_review.png'))
print(ART/'contact_sheet.png');print(ART/'before_after.png')
