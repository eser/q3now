# Font Sources

Place TTF files here before running `make build-fonts`:

| File | Font | License | Source |
|------|------|---------|--------|
| entsans.ttf | Enter Sansman Regular | Freeware | Included |
| entsani.ttf | Enter Sansman Italic | Freeware | Included |
| Oxanium-Regular.ttf | Oxanium Regular | SIL OFL 1.1 | [Google Fonts](https://fonts.google.com/specimen/Oxanium) |
| Oxanium-Medium.ttf | Oxanium Medium | SIL OFL 1.1 | [Google Fonts](https://fonts.google.com/specimen/Oxanium) |
| ShareTechMono-Regular.ttf | Share Tech Mono | SIL OFL 1.1 | [Google Fonts](https://fonts.google.com/specimen/Share+Tech+Mono) |

## Building atlases

```sh
make build-fonts
```

Outputs PNG + JSON atlas pairs to `modfiles/fonts/`.
