gulp-inline
===========

## Information

<table>
  <tr><td>Package</td><td>gulp-inline</td></tr>
  <tr><td>Description</td><td>Inlines js/css/svg into your html files</td></tr>
  <tr><td>Node Version</td><td>= 0.10</td></tr>
</table>


## Usage

```javascript
var inline = require('gulp-inline')
  , uglify = require('gulp-uglify')
  , minifyCss = require('gulp-minify-css');

gulp.src('public/index.html')
  .pipe(inline({
    base: 'public/',
    js: uglify,
    css: minifyCss,
    disabledTypes: ['svg', 'img', 'js'], // Only inline css files
    ignore: ['./css/do-not-inline-me.css']
  }))
  .pipe(gulp.dest('dist/'));
```

Replaces your &lt;script&gt; and &lt;link&gt; tags with the corresponding inlined files.

## SVG support

Currently there are two supported methods:

- `<img>` element with `src` attribute:

  ```
  <img src="/assets/icon.svg" class="MyClass" />
  ```

  which will be converted to something like

  ```
  <svg xmlns="http://www.w3.org/2000/svg" viewbox="..." class="MyClass">
    <circle ... />
    <path ... />
    etc.
  </svg>
  ```

  depending on what `/assets/icon.svg` contains

- `<svg>` element with `<use>` child element:

  ```
  <svg>
    <use xlink:href="/assets/icon.svg#MyIdentifier"></use>
  </svg>
  ```

  Note that this method requires an ID matching the ID of target SVG root element.

  This is valid target SVG for the example above:

  ```
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" id="MyIdentifier">
    ...
  </svg>
  ```

  while this is invalid:

  ```
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
    ...
  </svg>
  ```

## Options

Plugin options:

  * base - the root directory containing the files to be inlined
  * css - css transform (gulp plugin)
  * js - js transform (gulp plugin)
  * svg - svg transform (gulp plugin)
  * ignore - array of file paths to ignore and not inline (file paths as they appear in the source)
  * disabledTypes - array of types not to run inlining operations on (css, svg, js, img)
