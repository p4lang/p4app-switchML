# Documentation

The docs directory mainly contains files to generate the web documentation of SwitchML on read the docs.

You can click on [this](https://switchml.readthedocs.io) to read it online or build the documentation yourself as described in the following section.

## Building the documentation

To build the documentation you need three steps (Assuming you have already cloned the repository):

First, install the python requirements using this command (Assuming you are inside the docs directory):

    pip install -r requirements.txt

Second, install doxygen (For Ubuntu)

    sudo apt install doxygen

Third, run the docs makefile with the html target 

    make html

You will see lots of log messages and possibly many warnings but at the end you should see a message that the build succeeeded and that the HTML documentation pages are in _build/html.