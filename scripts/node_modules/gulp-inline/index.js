/**
 * Imports
 */

var through = require('through2')
var cheerio = require('cheerio')
var gulp = require('gulp')
var url = require('url')
var path = require('path')
var fs = require('fs')

/**
 * Inlinable type map
 */

var typeMap = {
  css: {
    tag: 'link',
    template: function (contents, el) {
      var attribute = el.attr('media')
      attribute = attribute ? ' media="' + attribute + '" ' : ''

      return '<style' + attribute + '>\n' + String(contents) + '\n</style>'
    },
    filter: function (el) {
      return el.attr('rel') === 'stylesheet' && isLocal(el.attr('href'))
    },
    getSrc: function (el) {
      return el.attr('href')
    }
  },

  js: {
    tag: 'script',
    template: function (contents, el) {
      var str = stringifyAttrs(without(el[0].attribs, 'src'))
      return '<script ' + str + '>\n' + String(contents) + '\n</script>'
    },
    filter: function (el) {
      return isLocal(el.attr('src'))
    },
    getSrc: function (el) {
      return el.attr('src')
    }
  },

  img: {
    tag: 'img',
    template: function (contents, el) {
      el.attr('src', 'data:image/unknown;base64,' + contents.toString('base64'))
      return cheerio.html(el)
    },
    filter: function (el) {
      var src = el.attr('src')
      return !/\.svg$/.test(src)
    },
    getSrc: function (el) {
      return el.attr('src')
    }
  },

  svg: {
    tag: ['img', 'svg', 'object'],
    template: function (contents, el) {
      var tag = el[0].tagName,
          $ = cheerio.load(String(contents), {decodeEntities: false})

      switch (tag) {
        case 'img':
          return cheerio.html($('svg').attr(without(el.attr(), 'src')))
          break
        case 'object':
          return cheerio.html($('svg').attr(without(el.attr(), 'data')))
          break
        case 'svg':
          return cheerio.html($('svg').removeAttr('id').attr(without(el.attr(), 'src')))
          break
      }
    },
    filter: function (el) {
      var tag = el[0].tagName

      switch (tag) {
        case 'img':
          var src = el.attr('src')
          return /\.svg$/.test(src) && isLocal(src)
          break
        case 'object':
          var src = el.attr('data')
          return /\.svg$/.test(src) && isLocal(src)
          break
        case 'svg':
          var children = el.children(),
              child    = children.first(),
              src      = child.attr('xlink:href')

          // Return TRUE if element contains only one child element and
          // that child element is a <use> element with xlink:href
          // attribute which refers to some random ID (which according to
          // HTML5 spec should contain at least one character and shouldn't
          // contain spaces) in external SVG file
          return children.length === 1
            && child[0].tagName === 'use'
            && /\.svg#\S+$/.test(src)
          break
      }
    },
    getSrc: function (el) {
      var tag = el[0].tagName

      switch (tag) {
        case 'img':
          return el.attr('src')
        case 'object':
          return el.attr('data')
        case 'svg':
          // Return "/foo.svg" out of "/foo.svg#SomeIdentifier"
          return el.children().first().attr('xlink:href').match(/[^#]+/)[0]
          break
      }
    }
  }
}

function inject ($, process, base, cb, opts, relative, ignoredFiles) {
  var items = []

  if (!process) {
    process = noop
  }

  // Normalize tags
  var tags = opts.tag instanceof Array ? opts.tag : [opts.tag]

  tags.forEach(function(tag) {
    $(tag).each(function (idx, el) {
      el = $(el)
      if (opts.filter(el)) {
        items.push(el)
      }
    })
  })

  if (items.length) {
    var done = after(items.length, cb)
    items.forEach(function (el) {
      var src = opts.getSrc(el) || ''
      var file = path.join(src[0] === '/' ? base : relative, src)

      if (fs.existsSync(file) && ignoredFiles.indexOf(src) === -1) {
        gulp.src(file)
          .pipe(process())
          .pipe(replace(el, opts.template))
          .pipe(through.obj(function (file, enc, cb) {
            cb()
          }, done))
      } else {
        done()
      }
    })
  } else {
    cb()
  }
}

/**
 * Inline plugin
 */

function inline (opts) {
  opts = opts || {}
  opts.base = opts.base || ''
  opts.ignore = opts.ignore || []
  opts.disabledTypes = opts.disabledTypes || []

  return through.obj(function (file, enc, cb) {
    var self = this
    var $ = cheerio.load(String(file.contents), {decodeEntities: false})
    var typeKeys = Object.getOwnPropertyNames(typeMap)
    var done = after(typeKeys.length, function () {
      file.contents = new Buffer($.html())
      self.push(file)
      cb()
    })

    typeKeys.forEach(function (type) {
      if (opts.disabledTypes.indexOf(type) === -1) {
        inject($, opts[type], opts.base, done, typeMap[type], path.dirname(file.path), opts.ignore)
      } else {
        done()
      }
    })
  })
}

/**
 * Utilities
 */

function replace (el, tmpl) {
  return through.obj(function (file, enc, cb) {
    el.replaceWith(tmpl(file.contents, el))
    this.push(file)
    cb()
  })
}

function noop () {
  return through.obj(function (file, enc, cb) {
    this.push(file)
    cb()
  })
}

function after (n, cb) {
  var i = 0
  return function () {
    i++
    if (i === n) cb.apply(this, arguments)
  }
}

function isLocal (href) {
  return href && href.slice(0, 2) !== '//' && !url.parse(href).hostname
}

function without (o, keys) {
  keys = [].concat(keys)
  return Object.keys(o).reduce(function (memo, key) {
    if (keys.indexOf(key) === -1) {
      memo[key] = o[key]
    }

    return memo
  }, {})
}

function stringifyAttrs (attrs) {
  return Object.keys(attrs).map(function (key) {
    return [key, attrs[key]].join('=')
  }).join(' ')
}

/**
 * Exports
 */

module.exports = inline
