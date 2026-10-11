# Patent research: automatic selection (Select Subject, Object Selection)

Claim-level record behind the "Automatic selection" bullet in [legal-constraints.md](legal-constraints.md) (review dated 2026-10-11; claims read on Google Patents; expiry dates are Google's adjusted estimates; not a legal opinion). Design: [ai-selection.md](ai-selection.md).

## Active patents and what each independent claim needs

- **US 10192129 B2** (Adobe, priority 2015-11-18, to about 2036-08): interactive deep selection. A network trained on images, masks, user indicators and distance maps; positive/negative clicked pixels; a distance map from the indicators to every pixel; image plus indicators plus distance map into the network. SAM encodes prompt coordinates positionally, with no distance map.
- **US 11314982 B2** (same family, to about 2037-01): a first user indicator inside the object and a second outside it, a network turning the image plus both indicators into a probability map, a mask from it. No distance map required. A positive and a negative point in one SAM solve read on it literally. **Never send a negative point.**
- **US 11568627 B2** (same family, to about 2037-06): two user inputs of different modalities (regional clicks, boundary box, language) through separate input channels of one multi-modal network. **One prompt type per solve: a box or a point, never both.**
- **US 12626496 B2** (same family, granted 2026-05-12): training with distance maps. Running a pretrained model does not practice it.
- **US 11676279 B2** (Adobe, priority 2020-12-18, to about 2041-02): an initial segmentation into one encoder, a user indicator converted to a distance map into a second encoder, a decoder producing the updated segmentation. **Never feed a previous mask back (SAM `mask_input` stays zero) and never refine a Select Subject result through the network.**
- **US 11244195 B2** (Adobe, priority 2018-05-01, to about 2040-05) and continuation **US 12591950 B2** (to about 2040-12): run a salient network on a reduced copy, crop the region it finds, run the network again, merge to full resolution; the continuation adds deciding whether more refinement is needed and re-processing sub-regions. **Select Subject is one pass over the whole image, upsampled once: no network-located crop, no second pass.** BiRefNet's internal fixed-grid patches are not a network-located region.
- **US 10460214 B2** (Adobe, to about 2038-01): a mobile device choosing a real-time or a static salient network by live feed versus still image.
- **US 11107219 B2, US 11631234 B2, US 11468550 B2, US 11055566** (Adobe, priority 2019, to about 2039): text queries, class labels, and choosing among detectors or known/unknown-class paths. **One fixed model per command, no class labels, no routing.**
- **US 11972569 B2** (Adobe, to about 2041-03): two segmentation models, merged overlapping masks, routing to specialist networks. **US 12260557 B2** (Adobe, to about 2043-06): per-region class-driven masking pipelines merged. Neither is practiced by a single fixed model.
- **US 2023/0129341 A1** (Adobe application 17/584,233, priority 2021-10-23; notice of allowance 2026-04-21, issue fee paid 2026-07-14, so probably issued; number not found): preliminary masks for several objects, an input over one object shows its mask, an input over another swaps to it. **No hover preview and no mask-everything-in-advance.** Watch item: find the issued number and allowed claims.
- **US 10068361 B2** (Adobe, to about 2035-11): a cursor over an indefinite boundary defines a matting band and adds soft selection when the input ends (the Refine Edge brush). **US 11593948 B2** (Adobe, to about 2041-05): a matting network driven by a guidance mask with two matte layers fused (claim 11 through a boundary-uncertainty mask). **No edge-refinement brush and no learned matting stage.**
- **US 10698588 B2** (Adobe, "Combined selection tool", to about 2037-09): automatically toggling between a coarse graph-cut mode and a level-set refinement mode from whether Add or Remove is active and whether the input lands inside or outside the current selection boundary. Narrower than "any input-driven algorithm switch"; still avoid switching mechanisms on mode plus click position.
- **US 8050498** (live coherent selection, to 2029-11-03): already binding for Quick Select; Object Selection likewise computes nothing for display during the drag.

## Assessment of the shipped design

- Select Subject: one BiRefNet pass over the whole image, sigmoid matte upsampled once, combined by plain boolean. Clear of the iterative-salient, detector, query and routing claims.
- Object Selection: one decoder run per release from one box (labels 2, 3) or one positive point (label 1 plus SAM's padding point, label -1). Clear of US 10192129 (no distance map; doctrine-of-equivalents risk arguable), US 11314982 (no negative indicator), US 11568627 (one modality), US 11676279 (no mask feedback). Picking the best of SAM's three point alternatives by predicted IoU chooses among outputs of one decoder run, not among algorithms; believed clear, unreviewed by counsel.
- The image encoder runs on the first press with no prompt and nothing displayed; the hover-preview application needs preliminary masks shown on hover, which never exist here.

## Prior art notes

GrabCut (2004) established box-to-segmentation. Single-pass CNN saliency (MDF 2015, DHSNet 2016, DSS 2017) predates US 11244195. Xu et al., "Deep Interactive Object Selection" (CVPR 2016) is the 10192129 family's own publication and postdates its priority, so it is not prior art against it. DEXTR (2018) and SAM (2023) only predate later filings.

## Not verified

Claims of US 11055566, US 11681919, US 11797847 and US 11468550 (abstracts only); the issued number of US 2023/0129341; any Meta filing on SAM (none found); Apple, Microsoft, Google, Canva/Affinity and Skylum portfolios beyond a light search.

Sources: patents.google.com pages for each number above; arXiv 2304.02643 (SAM).
