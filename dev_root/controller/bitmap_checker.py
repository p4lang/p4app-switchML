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
from bfrt_grpc.client import BfruntimeRpcException


class BitmapChecker(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(BitmapChecker, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get(
                'pipe.Ingress.update_and_check_worker_bitmap.update_and_check_worker_bitmap'
            )
        ]
        self.table = self.tables[0]

        self.register = bfrt_info.table_get(
            'pipe.Ingress.update_and_check_worker_bitmap.worker_bitmap')

        # Clear register
        self._clear()

    def _clear(self):
        ''' Clear only bitmap register '''
        self.register.entry_del(self.target)

    def get_bitmap(self, start=0, count=None):
        ''' Get the current bitmap values.
            The parameters can limit the number of returned indices to
            [start, start + count]. The default is [0, 8].
        '''

        if count == None:
            count = 8

        try:
            resp = self.register.entry_get(self.target, [
                self.register.make_key([self.gc.KeyTuple('$REGISTER_INDEX', i)])
                for i in range(start, start + count)
            ],
                                           flags={'from_hw': True})

            values = []
            for v, k in resp:
                v = v.to_dict()
                k = k.to_dict()

                set0 = v[
                    'Ingress.update_and_check_worker_bitmap.worker_bitmap.first']
                set1 = v[
                    'Ingress.update_and_check_worker_bitmap.worker_bitmap.second']

                # Separate values for each pipe
                for pipe, (s0, s1) in enumerate(zip(set0, set1)):
                    entry_set0 = {
                        'index': k['$REGISTER_INDEX']['value'],
                        'set': 0,
                        'pipe': pipe,
                        'bitmap': s0
                    }
                    entry_set1 = {
                        'index': k['$REGISTER_INDEX']['value'],
                        'set': 1,
                        'pipe': pipe,
                        'bitmap': s1
                    }
                    values.extend([entry_set0, entry_set1])

        except BfruntimeRpcException as bfrte:
            # Indices out of bound
            self.log.debug(str(bfrte))
            return []

        return values
