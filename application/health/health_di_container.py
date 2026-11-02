from dependency_injector.providers import Factory

from application.health.health_endpoint import HealthHandler
from infrastructure import BaseDiContainer
from infrastructure.infrastructure_injector import InfrastructureDiContainer


class HealthDiContainer(BaseDiContainer):
    health_handler = Factory(
        HealthHandler,
        database=InfrastructureDiContainer.database,
        mongo_client=InfrastructureDiContainer.mongo_db_client,
    )
