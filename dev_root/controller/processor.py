import logging

from control import Control


class Processor(Control):

    def __init__(self, target, gc, bfrt_info, n):
        # Set up base class
        super(Processor, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.value{:02d}.sum'.format(n))
        ]
        self.table = self.tables[0]

        self.register = bfrt_info.table_get(
            'pipe.Ingress.value{:02d}.values'.format(n))

        # Clear register
        self._clear()

    def _clear(self):
        ''' Clear only processor register '''
        self.register.entry_del(self.target)
