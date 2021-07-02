#  Copyright 2021 Intel-KAUST-Microsoft
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# SwitchML Project
# @file docs_setup.py
# @brief This is a simple sphinx extension that brings in and prepares documents scattered across the repository

import os
from pathlib import Path

def setup(app):
    input_path_prefix = "../"
    readmes_path_prefix = "readmes/"

    Path(readmes_path_prefix).mkdir(parents=True, exist_ok=True)

    on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

    # All the readme files that we want to copy in the docs 
    readme_mappings = {
        "README.md": "overview.md",
        "CONTRIBUTING.md": "contrib.md",
        "LICENSE": "license.md",
        "dev_root/client_lib/README.md": "client_lib.md",
        "dev_root/examples/README.md": "examples.md",
        "dev_root/benchmarks/README.md": "benchmarks.md",
        "dev_root/p4/README.md": "p4.md",
        "dev_root/controller/README.md": "controller.md",
        "dev_root/frameworks_integration/README.md": "frameworks_integration.md",
        "dev_root/frameworks_integration/pytorch_patch/README.md": "pytorch_patch.md",
        "dev_root/frameworks_integration/nccl_plugin/README.md": "nccl_plugin.md"
    }

    # We might need different links for three different use cases:
    # 1. Browsing the repo on github: In this case we want the links to point to github folders and files.
    # 2. Browsing the documentation on read the docs: In this case we want the links to point the read the docs pages.
    # 3. Generating and browsing the documentation locally: In this case we want the links to point to the locally generated html pages.
    #
    # The readmes are by default written to address case 1. Therefore for the other 2 cases we need to provide a link mapping.

    hyperlink_mappings = {
        "/dev_root/p4": "p4",
        "/dev_root/controller": "controller",
        "/dev_root/client_lib": "client_lib",
        "/dev_root/examples": "examples",
        "/dev_root/benchmarks": "benchmarks",
        "/CONTRIBUTING.md": "contrib",
        "/LICENSE": "license",
        "/dev_root/frameworks_integration": "frameworks_integration",
        "/dev_root/frameworks_integration/pytorch_patch": "pytorch_patch",
        "/dev_root/frameworks_integration/nccl_plugin": "nccl_plugin",
        "/docs/img/benchmark.png": "../../../../img/benchmark.png"
    }

    if on_rtd:
        # Update any links particular to RTD here.
        hyperlink_mappings["/docs/img/benchmark.png"] = "https://raw.githubusercontent.com/OasisArtisan/p4app-switchML/main/docs/img/benchmark.png"

    print("Copying readme files from the repository and preparing them for RTD.")
    for infile, outfile in readme_mappings.items():
        with open(input_path_prefix + infile, "r") as f:
            intext = "".join(f.readlines())

        outtext = intext

        for original, replacement in hyperlink_mappings.items():
            if infile.endswith(".md"):
                # Regex might be better. But this is good enough for now.
                outtext = outtext.replace("]({})".format(original), "]({})".format(replacement))
            else:
                outtext = outtext.replace(original, replacement)

        with open(readmes_path_prefix + outfile, "w") as f:
            f.write(outtext)
