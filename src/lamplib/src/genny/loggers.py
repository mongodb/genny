from io import StringIO
import re
import sys

import structlog
import colorama as c
from typing import Any


def setup_logging(verbose: bool = False) -> None:
    import logging

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
    logging.getLogger("asyncio").setLevel(logging.INFO)
    logging.getLogger("blib2to3").setLevel(logging.WARN)

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
            StringifyAndRedact(),
            CustomConsoleRenderer(pad_event=20, colors=True, force_colors=True),
        ],
        context_class=dict,
        logger_factory=structlog.stdlib.LoggerFactory(),
        wrapper_class=structlog.stdlib.BoundLogger,
        cache_logger_on_first_use=True,
    )
    # Initialize colorama. Structlog does this but it doesn't have the strip=False
    # so we don't get colors on Evergreen pages (which usually doesn't give us a TTY).
    c.init(strip=False)  # Don't strip ansi colors even if we're not on a tty.


class StringifyAndRedact:
    """
    Force all values in `event_dict` to be string, and redact secrets in those values.
    This ensures that all secrets are redacted, regardless of how nested they are in
    data structures. (Custom)ConsoleRenderer would convert all the values to string in order
    to print them to stdout anyway.

    Put this in the processor chain just before (Custom)ConsoleRenderer.
    """

    regex_to_redaction: dict[re.Pattern[str], str]

    def __init__(self) -> None:
        self.regex_to_redaction = {
            re.compile(r"://([^:@]*):([^@]*)@"): r"://\g<1>:[REDACTED]@",  # password in URLs
        }

    def __call__(self, logger: Any, name: str, event_dict: dict) -> dict:
        for key, value in event_dict.items():
            for regex, redaction in self.regex_to_redaction.items():
                event_dict[key] = re.sub(regex, redaction, str(value))
        return event_dict


class CustomConsoleRenderer(structlog.dev.ConsoleRenderer):
    def __call__(self: structlog.dev.ConsoleRenderer, _: Any, __: Any, event_dict: dict) -> str:
        """
        Unfortunately structlog's ConsoleRenderer doesn't let us format the log message.
        This changes the format by overriding the __call__ method.

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
        # Initialize lazily to prevent import side-effects.
        if self._init_colorama:
            structlog.dev._init_colorama(self._force_colors)
            self._init_colorama = False
        try:
            sio = StringIO()

            # Genny logging is nearly always wrapped by other logging (e.g. dsi logging) so no need for additional timestamp info
            # ts = event_dict.pop("timestamp", None)
            # if ts is not None:
            #     sio.write(
            #         # can be a number if timestamp is UNIXy
            #         self._styles.timestamp
            #         + str(ts)
            #         + self._styles.reset
            #         + " "
            #     )

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
                    + structlog.dev._pad(logger_name, 20)
                    + self._styles.reset
                    + "] "
                )

            event = event_dict.pop("event")
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
