from dependency_injector.providers import Singleton, Resource

from infrastructure import PsqlDatabase
from infrastructure.di.base_di_container import BaseDiContainer
from infrastructure.http import HttpClient, HttpClientConfig
from infrastructure.mongo import MongoDbConfig, MongoDbClient, MongoJsonDatabase, MongoRawDataDatabase
from infrastructure.postgres.database import PsqlConfig

class InfrastructureDiContainer(BaseDiContainer):
    http_client_config = Resource(
        HttpClientConfig,
        user_agent=BaseDiContainer.config.http_client_config.user_agent,
        connect_timeout_seconds=BaseDiContainer.config.http_client_config.connect_timeout_seconds.as_int(),
        read_timeout_seconds=BaseDiContainer.config.http_client_config.read_timeout_seconds.as_int(),
    )

    http_client = Singleton(HttpClient, config=http_client_config)

    db_config : Resource[PsqlConfig] = Resource(
        PsqlConfig,
        host=BaseDiContainer.config.database.host,
        port=BaseDiContainer.config.database.port,
        username=BaseDiContainer.config.database.username,
        password=BaseDiContainer.config.database.password,
        database=BaseDiContainer.config.database.database
    )

    database = Singleton(
        PsqlDatabase,
        config=db_config
    )
    
    mongo_db_config = Resource(
        MongoDbConfig,
        host=BaseDiContainer.config.mongo_db_config.host,
        port=BaseDiContainer.config.mongo_db_config.port,
        username=BaseDiContainer.config.mongo_db_config.username,
        password=BaseDiContainer.config.mongo_db_config.password,
        raw_data_database_name=BaseDiContainer.config.mongo_db_config.raw_data_database_name,
        json_data_database_name=BaseDiContainer.config.mongo_db_config.json_data_database_name
    )
    
    mongo_db_client = Singleton(
        MongoDbClient,
        config=mongo_db_config
    )
    
    mongo_json_db = Singleton(
        MongoJsonDatabase,
        config=mongo_db_config,
        mongo_client=mongo_db_client
    )

    mongo_raw_data_db = Singleton(
        MongoRawDataDatabase,
        config=mongo_db_config,
        mongo_client=mongo_db_client
    )
