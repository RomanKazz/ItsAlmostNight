# Selection contours at occluder contacts

Selection contours use the mask's sampleable depth attachment to compare the
chosen silhouette sample against visible scene depth. The former RGBA8 inverse
depth alpha could not distinguish a slope a few centimetres in front of a
selected object. Its 1.5/255 tolerance let a white seam form along the occluder.

The selection target now uses the existing depth-texture allocation path instead
of a depth renderbuffer. The colour mask and effect-coverage encoding stay the
same. There is no extra render pass, mask or geometry draw. Precise comparison
uses a 2 mm tolerance and also checks explicit black mask occluders; the outer
visible silhouette remains outlined. The additional depth lookups run only on
candidate contour pixels after empty/filled mask fragments have been rejected.

The render-performance regression draws a selected box partially covered by a
sloped foreground surface just 2.5 cm closer than its face. It exercises both
scene-depth-only and black-occluder mask paths. Before the fix the seeded path
produced 90 seam pixels; afterwards both paths produce zero while retaining
over 100 exterior contour pixels. Captures can be generated with
`IAN_CAPTURE_SELECTION_OCCLUSION=/tmp/selection-contact build/debug/ian_render_performance_smoke`.

No public API or save format changes. This changes selection outlines only;
the ordinary world ink shader and interaction targeting remain unchanged.

Validation: Debug/Release builds passed, Debug CTest 42/42, Release render
performance and interaction UI checks 2/2. Existing foreground fire/selection
checks passed in the graphics suite. Before/after captures were inspected.
