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

from control import Control


class WorkersCounter(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(WorkersCounter, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.workers_counter.count_workers')
        ]
        self.table = self.tables[0]

        self.register = bfrt_info.table_get(
            'pipe.Ingress.workers_counter.workers_count')

        # Clear register
        self._clear()

    def _clear(self):
        ''' Clear only workers count register '''
        self.register.entry_del(self.target)
