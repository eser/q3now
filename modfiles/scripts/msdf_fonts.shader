// MSDF font atlas shaders -- registered via RE_RegisterMSDFShader()
// These define the base material properties; the MSDF fragment program
// is applied by the renderer when shader_t.msdf == qtrue.

// ── Enter Sansman ──────────────────────────────────────────────
// Each weight has its own atlas (FontForge-transformed at build time)

fonts/sansman-regular_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sansman-regular.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

fonts/sansman-medium_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sansman-medium.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

fonts/sansman-bold_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sansman-bold.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

fonts/sansman-italic_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sansman-italic.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

fonts/sansman-bold-italic_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sansman-bold-italic.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

// ── Oxanium ────────────────────────────────────────────────────

fonts/oxanium_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/oxanium.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

fonts/oxanium-medium_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/oxanium-medium.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}

// ── Share Tech Mono ────────────────────────────────────────────

fonts/sharetechmono_atlas
{
    nopicmip
    nomipmaps
    {
        map fonts/sharetechmono.png
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen vertex
        alphaGen vertex
    }
}
