import logging

from control import Control


class Exponents(Control):

    def __init__(self, target, gc, bfrt_info):
        # Set up base class
        super(Exponents, self).__init__(target, gc)

        self.log = logging.getLogger(__name__)

        self.tables = [
            bfrt_info.table_get('pipe.Ingress.exponents.exponent_max')
        ]
        self.table = self.tables[0]

        self.register = bfrt_info.table_get('pipe.Ingress.exponents.exponents')

        # Clear register
        self._clear()

    def _clear(self):
        ''' Clear only exponent register '''
        self.register.entry_del(self.target)
