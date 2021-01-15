import logging
import structlog
import colorama as c
from io import StringIO

from typing import Any

import sys


def setup_logging(verbose: bool = False) -> None:
    """Configure logging verbosity and destination."""

    loglevel = logging.DEBUG if verbose else logging.INFO

    handler = logging.StreamHandler(stream=sys.stdout)
    handler.setLevel(loglevel)

    root_logger = logging.getLogger()
    root_logger.setLevel(loglevel)
    root_logger.addHandler(handler)

    # The following sets the minimum level that we will see for the given component. So we will
    # only see warnings and higher for paramiko, boto3 and botocore. We will only see errors / fatal
    # / critical log messages for /dev/null
    logging.getLogger("paramiko").setLevel(logging.WARNING)
    logging.getLogger("boto3").setLevel(logging.WARNING)
    logging.getLogger("botocore").setLevel(logging.WARNING)
    logging.getLogger("error_only").setLevel(logging.ERROR)

    structlog.configure_once(
        processors=[
            structlog.stdlib.filter_by_level,
            structlog.stdlib.add_logger_name,
            structlog.stdlib.add_log_level,
            structlog.processors.StackInfoRenderer(),
            # Same as 'iso' format but without the useless milliseconds
            structlog.processors.TimeStamper(fmt="%Y-%m-%dT%H:%M:%SZ"),
            structlog.processors.format_exc_info,
            structlog.processors.UnicodeDecoder(),
            structlog.dev.ConsoleRenderer(pad_event=20, colors=True, force_colors=True),
        ],
        context_class=dict,
        logger_factory=structlog.stdlib.LoggerFactory(),
        wrapper_class=structlog.stdlib.BoundLogger,
        cache_logger_on_first_use=True,
    )
    # Initialize colorama. Structlog does this but it doesn't have the strip=False
    # so we don't get colors on Evergreen pages (which usually doesn't give us a TTY).
    c.init(strip=False)  # Don't strip ansi colors even if we're not on a tty.

    _tweak_structlog_log_line()
    _LOGGING_SETUP = True


def _tweak_structlog_log_line() -> None:
    """
    Unfortunately structlog's ConsoleRenderer doesn't give us any ability to format the log message.
    This changes the format by monkeypatching the __call__ method.

    Default:
        timestamp [level] event [logger] params exc_info
    Changed to:
        timestamp [level] [logger] event params exc_info

    Also tweaked a couple padding values:

      level:  Default pads to longest level (e.g. len('exception')=9).
              Changed to pad to 5 (we usually only use info and debug)

      logger: Default pads to 30.
              Changed from 30 to 27 after running a few common DSI commands.

    :return: None
    """

    def _override_call(
        self: structlog.dev.ConsoleRenderer, _: Any, __: Any, event_dict: dict
    ) -> str:
        # Initialize lazily to prevent import side-effects.
        if self._init_colorama:
            structlog.dev._init_colorama(self._force_colors)
            self._init_colorama = False
        try:
            sio = StringIO()

            ts = event_dict.pop("timestamp", None)
            if ts is not None:
                sio.write(
                    # can be a number if timestamp is UNIXy
                    self._styles.timestamp
                    + str(ts)
                    + self._styles.reset
                    + " "
                )

            level = event_dict.pop("level", None)
            if level is not None:
                sio.write(
                    "["
                    + self._level_to_color[level]
                    + structlog.dev._pad(level, 5)
                    + self._styles.reset
                    + "] "
                )

            logger_name = event_dict.pop("logger", None)
            if logger_name is not None:
                sio.write(
                    "["
                    + self._styles.logger_name
                    + self._styles.bright
                    + structlog.dev._pad(logger_name, 27)
                    + self._styles.reset
                    + "] "
                )

            # force event to str for compatibility with standard library
            event = event_dict.pop("event")
            if not isinstance(event, str):
                event = str(event)

            if event_dict:
                event = structlog.dev._pad(event, self._pad_event) + self._styles.reset + " "
            else:
                event += self._styles.reset
            sio.write(self._styles.bright + event)

            stack = event_dict.pop("stack", None)
            exc = event_dict.pop("exception", None)
            sio.write(
                " ".join(
                    self._styles.kv_key
                    + key
                    + self._styles.reset
                    + "="
                    + self._styles.kv_value
                    + self._repr(event_dict[key])
                    + self._styles.reset
                    for key in sorted(event_dict.keys())
                )
            )

            if stack is not None:
                sio.write("\n" + stack)
                if exc is not None:
                    sio.write("\n\n" + "=" * 79 + "\n")
            if exc is not None:
                sio.write("\n" + exc)

            out = sio.getvalue()
            return out
        finally:
            sio.close()

    structlog.dev.ConsoleRenderer.__call__ = _override_call
