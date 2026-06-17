# climetadata

A lightweight CLI utility to recursively scrub metadata from image, document, and audio files while preserving file content and quality.

## Features

- Recursively scans directories for supported file types
- Strips metadata from `.jpg`, `.jpeg`, `.png`, `.pdf`, `.mp3`, and `.wav`
- Uses a thread pool for high throughput
- Minimal dependencies, optimized for direct binary processing

## Installation

Build from source using `make`:

```bash
make
```

## Usage

```bash
./climetadata /path/to/target-directory
```

The utility walks the specified directory tree and scrubs metadata in-place.

## License

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
