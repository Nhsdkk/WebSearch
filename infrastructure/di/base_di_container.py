import sys
from pathlib import Path

from dependency_injector.containers import DeclarativeContainer
from dependency_injector.providers import Configuration

class BaseDiContainer(DeclarativeContainer):
    PROJECT_PATH = Path(sys.argv[0]).absolute().parent
    CONFIGURATION_PATH = f"{PROJECT_PATH}/config.yaml"

    config = Configuration(strict=True, yaml_files=[CONFIGURATION_PATH])