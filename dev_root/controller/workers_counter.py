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
