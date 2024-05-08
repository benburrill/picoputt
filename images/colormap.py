# Usage: python3 colormap.py [<spec>] [<outfile>]
#  A script for creating colormap textures from a matplotlib-compatible
#  Colormap object.
#
#  <spec>      Colormap specifier, with the same syntax as setuptools
#              entry-point specifiers.  [Default: matplotlib.cm:plasma]
#              If the : is omitted, the module defaults to matplotlib.cm
#
#  <outfile>   Output file.  [Default: colormap_{cmap_name}.bmp]
#              NOTE: picoputt expects the colormap texture to be named
#              colormap.bmp

if __name__ == '__main__':
    import sys
    from importlib import import_module
    from functools import reduce
    from PIL import Image

    cmap_spec = (sys.argv[1:] or ['plasma'])[0]
    parts = cmap_spec.split(':', 1)
    cmap_mod, cmap_name = parts if len(parts) > 1 else ('matplotlib.cm', parts[0])
    cmap = reduce(getattr, cmap_name.split('.'), import_module(cmap_mod))

    dest_file = (sys.argv[2:] or [f'colormap_{cmap_name}.bmp'])[0]
    Image.fromarray(cmap([range(cmap.N)], bytes=True)).save(dest_file)
