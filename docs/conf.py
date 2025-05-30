# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'TeleportXR'
copyright = '2018-2025, Teleport XR Ltd'
author = 'Roderick Kennedy'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [ "breathe","myst_parser","sphinxcontrib.mermaid", "sphinx.ext.graphviz","sphinxcontrib.jquery"]

mermaid1_output_format='png'

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
include_patterns=['*.rst','libavstream/*.rst','libavstream/**/*.rst','libavstream/**/*.png','TeleportCore/*.rst','TeleportCore/**','TeleportServer/*.rst','TeleportServer/**'
                    ,'TeleportClient/**','docs/**','docs/*.rst','**/*.png','**/*.svg','client/*.rst','client/**/*.rst']
exclude_patterns = ['build*']

html_static_path = ['_static']
html_logo = "docs/images/TeleportLogo-64.png"
html_theme_options = {
    'logo_only': False,
    'display_version': False,
    'navigation_depth':2
}
numfig = True
html_favicon = 'client/assets/textures/favicon.ico'

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'teleport_sphinx_theme'
html_theme_path = ["."]

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

breathe_default_project = "TeleportXR"
breathe_show_include = True
# Don't ignore cpp files if that's where the documentation is found.
breathe_implementation_filename_extensions = []

cpp_index_common_prefix = ['avs::','teleport::','teleport::core','teleport::server','teleport::client','teleport::clientrender']

# npm install -g @mermaid-js/mermaid-cli
mermaid_output_format ='png'
mermaid_cmd='mmdc.cmd'
