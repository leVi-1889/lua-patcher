from PIL import Image

img = Image.open('icon.png').convert('RGBA')
sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
images = [img.resize(s, Image.Resampling.LANCZOS) for s in sizes]
images[0].save('icon.ico', format='ICO', sizes=sizes, append_images=images[1:])
print('Done HQ')
