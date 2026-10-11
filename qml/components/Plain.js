.pragma library

// Silica's PageHeader renders a title that looks like markup as rich
// text, and a title from a music file or a server could then make the app
// load a remote image. A zero-width space after "<" keeps such text plain
// and looks the same.
function text(value) {
    return String(value === undefined || value === null ? "" : value).replace(/</g, "<​")
}
