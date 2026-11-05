# fm1song: diagnostics. Licence: TBD (see D017)
#
# Core code never prints or exits: it records problems in a Diagnostics object (errors and warnings, each
# with the file and, where it applies, the place in the song as bar:beat) and raises SongError when the song
# cannot be used. The CLI (and later a website) decides how to show them.


class Diagnostic:
    def __init__(self, level, message, file=None, where=None):
        self.level = level            # "error" | "warning"
        self.message = message
        self.file = file              # "song.yaml", "song.mid", "song.fm1s", ...
        self.where = where            # "chart.verse", "track bass:2 at 12:3", ...

    def __str__(self):
        loc = ": ".join(x for x in (self.file, self.where) if x)
        return f"{self.level}: {loc + ': ' if loc else ''}{self.message}"

    def as_dict(self):
        return {"level": self.level, "message": self.message, "file": self.file, "where": self.where}


class SongError(Exception):
    """The song (or a compiled file) cannot be used. .diagnostics lists every error found so far."""

    def __init__(self, diagnostics):
        self.diagnostics = list(diagnostics)
        errors = [d for d in self.diagnostics if d.level == "error"] or self.diagnostics
        super().__init__("\n".join(str(d) for d in errors))


class Diagnostics:
    def __init__(self):
        self.items = []

    def error(self, message, file=None, where=None):
        self.items.append(Diagnostic("error", message, file, where))

    def warn(self, message, file=None, where=None):
        self.items.append(Diagnostic("warning", message, file, where))

    def fatal(self, message, file=None, where=None):
        """record an error and stop: the rest of the song cannot be checked without this part"""
        self.error(message, file, where)
        raise SongError(self.items)

    @property
    def errors(self):
        return [d for d in self.items if d.level == "error"]

    @property
    def warnings(self):
        return [d for d in self.items if d.level == "warning"]

    def check(self):
        """raise SongError if any error was recorded"""
        if self.errors:
            raise SongError(self.items)
