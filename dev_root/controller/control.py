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
