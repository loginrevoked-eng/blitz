
import re


class Envfile:
    _LINE_RE = re.compile(r"^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$")

    @staticmethod
    def is_env(text: str, threshold: float = 0.8) -> bool:
        assignments = 0
        nonempty = 0

        for line in text.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            nonempty += 1
            if Envfile._LINE_RE.match(line):
                assignments += 1

        return assignments >= 2 and assignments / max(nonempty, 1) >= threshold

    @staticmethod
    def parse(text: str, threshold: float = 0.8) -> dict[str, str] | None:
        if not Envfile.is_env(text, threshold):
            return None

        result = {}
        for line in text.splitlines():
            match = Envfile._LINE_RE.match(line)
            if match:
                key, value = match.groups()
                value = value.strip()
                if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                    value = value[1:-1]
                result[key] = value

        return result
