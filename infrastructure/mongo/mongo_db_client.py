import uuid
from dataclasses import dataclass
from typing import Any

from gridfs import GridFS
from pymongo import MongoClient
from pymongo.synchronous.database import Database


@dataclass
class MongoDbConfig:
    host: str
    port: int
    username: str
    password: str
    
    raw_data_database_name: str
    json_data_database_name: str

    @property
    def connection_string(self) -> str:
        return f"mongodb://{self.username}:{self.password}@{self.host}:{self.port}/?directConnection=true"

class MongoDbClient:
    __client: MongoClient
    
    def __init__(self, config: MongoDbConfig):
        self.__client = MongoClient(config.connection_string)
    
    def get_database[TDocument](self, database_name: str) -> Database[TDocument]:
        return self.__client.get_database(database_name)

class MongoJsonDatabase[TDocument]:
    db: Database[TDocument]
    
    def __init__(
        self,
        config: MongoDbConfig,
        mongo_client: MongoClient):
        self.db = mongo_client.get_database(config.json_data_database_name)
        
class MongoRawDataDatabase:
    db: GridFS
    
    def __init__(self, config: MongoDbConfig, mongo_client: MongoClient):
        database = mongo_client.get_database(config.raw_data_database_name)
        self.db = GridFS(database)
        
    def upsert_file(self, file_id: uuid.UUID, content: Any) -> None:
        string_file_id = str(file_id)
        existing_file = self.db.find_one({"_id": string_file_id})

        if existing_file is not None:
            self.db.delete(string_file_id)

        self.db.put(content, _id=str(file_id))