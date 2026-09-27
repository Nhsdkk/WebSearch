from typing import Callable, Any

from sqlalchemy import create_engine, Engine

from dataclasses import dataclass
from enum import Enum

from sqlalchemy.orm import Session


@dataclass(frozen=True)
class PsqlConfig:
    host: str
    port: int
    username: str
    password: str
    database: str
    
    @property
    def connection_string(self):
        return f"postgresql+psycopg2://{self.username}:{self.password}@{self.host}:{self.port}/{self.database}"

class PsqlDatabase:
    _db: Engine
    
    def __init__(self, config: PsqlConfig):
        self._db = create_engine(config.connection_string, echo=True)
        
    def create_session(self) -> Session:
        return Session(self._db)

    def __delete__(self, instance):
        self._db.dispose()