// Optional asset preparation tool, not a dependency of the game/build.
// Requires @resvg/resvg-js. The original Figma SVGs are never rewritten.
const fs = require('node:fs');
const path = require('node:path');
const { Resvg } = require('@resvg/resvg-js');

const directory = path.resolve(__dirname, '../assets/ui/main_menu/figma');
const polygons = [2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15];
function rasterize(name, source) {
    // The exported group applies its inner highlight to the border as well
    // as the fill. Composite the original stroke above the filtered group,
    // preserving its exact path/width and leaving the source SVG unchanged.
    const strokes = source.match(/<path\b[^>]*\bstroke="black"[^>]*\/>/g);
    if (!strokes || strokes.length !== 1) {
        throw new Error(`${name} must have exactly one original black outline`);
    }
    const outlinedSource = source.replace('</svg>', `${strokes[0]}</svg>`);
    const image = new Resvg(outlinedSource, {
        fitTo: { mode: 'zoom', value: 2 },
        background: 'rgba(0,0,0,0)',
        font: { loadSystemFonts: false },
    }).render();
    if (image.pixels[3] !== 0) throw new Error(`${name} has an opaque corner`);
    const top = (2 * image.width + Math.floor(image.width / 2)) * 4;
    if (image.pixels[top] !== 0 || image.pixels[top + 1] !== 0 ||
        image.pixels[top + 2] !== 0 || image.pixels[top + 3] !== 255) {
        throw new Error(`${name} must have a solid black top outline`);
    }
    fs.writeFileSync(path.join(directory, `${name}.png`), image.asPng());
    console.log(`${name}: ${image.width}x${image.height}, transparent`);
}
for (const number of polygons) {
    const name = `imgPolygon${number}`;
    const source = fs.readFileSync(path.join(directory, `${name}.svg`), 'utf8');
    rasterize(name, source);
    if ([2, 3, 4, 5, 7, 8, 9].includes(number)) {
        // Selection changes only the fill; geometry, effects and outline
        // still come from the original Figma node.
        rasterize(`${name}_selected`, source.replace(/fill="#(?:54595F|F0BC1E)"/g,
            'fill="#F0BC1E"'));
        if (number === 2) rasterize(`${name}_inactive`,
            source.replace('fill="#F0BC1E"', 'fill="#54595F"'));
    }
}
