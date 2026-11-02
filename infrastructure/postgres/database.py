from typing import Callable, Any

from sqlalchemy import create_engine, Engine, text
from sqlalchemy.pool import NullPool

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
        self._health_db = create_engine(
            config.connection_string,
            poolclass=NullPool,
            connect_args={"connect_timeout": 2, "options": "-c statement_timeout=2000"},
        )
        
    def create_session(self) -> Session:
        return Session(self._db)

    def check_health(self) -> bool:
        # Use a separate connection so probes cannot wait on the workers' connection pool.
        with self._health_db.connect() as connection:
            return connection.scalar(text("SELECT 1")) == 1

    def __delete__(self, instance):
        self._db.dispose()
        self._health_db.dispose()
