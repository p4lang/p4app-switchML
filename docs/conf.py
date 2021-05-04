# SwitchML Project
# @file setup.py
# @brief This is a simple sphinx extension that brings in and prepares documents scattered across the repository
#
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

import os
import sys

sys.path.insert(0, os.path.abspath('.'))

# -- Project information -----------------------------------------------------

project = 'SwitchML'
copyright = '2021, SwitchML'
author = 'SwitchML'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "docs_setup",
    "myst_parser",
    "sphinx_rtd_theme",
    "breathe",
    "exhale"
]

# Setup the breathe extension
breathe_projects = {
    "SwitchML": "_build/doxy_output/xml"
}
breathe_default_project = "SwitchML"

# Doxygen configuration
doxyconf = [
    "INPUT = ../dev_root/client_lib/src/",
    "FILE_PATTERNS = *.h",
    "EXCLUDE = ../dev_root/client_lib/src/glog_fix.h",
    "PREDEFINED += DPDK RDMA TIMEOUTS"
]

# Setup the exhale extension
exhale_args = {
    # These arguments are required
    "containmentFolder":     "./client_lib_api", # For some reason this folder cannot be inside the build directory
    "rootFileName":          "client_lib_root.rst",
    "rootFileTitle":         "Client Library API",
    "doxygenStripFromPath":  "..",
    # Suggested optional arguments
    "createTreeView":        True,
    # TIP: if using the sphinx-bootstrap-theme, you need
    # "treeViewIsBootstrap": True,
    "exhaleExecutesDoxygen": True,
    "exhaleDoxygenStdin": "\n".join(doxyconf)
}

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'cpp'

# Add any paths that contain templates here, relative to this directory.
templates_path = []

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
# on_rtd is whether we are on readthedocs.org, this line of code grabbed from docs.readthedocs.org
on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

if not on_rtd:  # only import and set the theme if we're building docs locally
    import sphinx_rtd_theme
    html_theme = 'sphinx_rtd_theme'
    html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = []