# Definition validation parity

VIA's Design tab calls the four AJV validators exported by
`@the-via/reader`: keyboard-definition and transformed-VIA-definition shapes
for both V2 and V3. It uses the version selected in Settings; it does not
automatically try the other version.

TabVIA follows the same selection and four-shape model. The embedded validator
currently covers JSON syntax, identity, matrix, layout container, V2 lighting
presence and V3 lighting exclusion. It emits reader-style instance paths.
Nested KLE key, label, preset, menu, custom-keycode and no-extra-properties
constraints remain to be ported. Therefore it is not yet correct to claim full
reader validation parity.

`tools/validate-with-reader.mjs` uses the actual reader version pinned by the
reference VIA app. Use it to build a fixture corpus:

```sh
npm install
npm run validate:reader -- v3 keyboard.json
```

Every embedded rule must have fixtures whose accept/reject result matches this
harness. Reader version and VIA app reference commits are pinned in the parity
document so upstream changes can be reviewed deliberately.
