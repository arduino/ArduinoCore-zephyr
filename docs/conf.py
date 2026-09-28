# Copyright (c) Infineon Technologies AG
#
# SPDX-License-Identifier: Apache-2.0

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

import os

# Check if we're running on Read the Docs' servers
read_the_docs_build = os.environ.get("READTHEDOCS", None) == "True"

# -- Project information -----------------------------------------------------

project = "Arduino Core for Zephyr"
copyright = "2026 Infineon Technologies AG"
author = "Infineon Technologies AG"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx_tabs.tabs",
    "sphinxemoji.sphinxemoji",
    "myst_parser",
]

autosectionlabel_prefix_document = True

source_suffix = [
    ".rst",
]

suppress_warnings = ["autosectionlabel.*", "epub.duplicated_toc_entry"]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# Tell sphinx what the primary language being documented is.
primary_domain = "cpp"

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["_build", "build", "Thumbs.db", ".DS_Store"]

highlight_language = "c++"

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
html_theme = "sphinx_rtd_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_templates"]
