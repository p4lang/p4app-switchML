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

import logging


class Control(object):

    def __init__(self, target, gc):

        # Get logging, client, and global program info
        self.log = logging.getLogger(__name__)
        self.gc = gc
        self.target = target

        # Child classes must set tables
        self.tables = None

    def _clear(self):
        ''' Remove all existing entries in the tables '''
        if self.tables is not None:
            for table in self.tables:
                table.entry_del(self.target)
                table.default_entry_reset(self.target)
