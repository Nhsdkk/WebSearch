from dataclasses import dataclass
from datetime import datetime
from typing import Self


@dataclass
class ProcessedPage:
    COLLECTION_NAME = "page_content"
    
    page_id: str
    title: str
    text_content: str
    processed_at: datetime
    
    def to_mongo_db(self) -> dict:
        return {
            "_id": self.page_id,
            "page_id": self.page_id,
            "title": self.title,
            "text_content": self.text_content,
            "processed_at": self.processed_at,
        }

    @classmethod
    def from_mongo_db(cls, data: dict) -> Self:
        return cls(
            page_id=data["page_id"],
            title=data["title"],
            text_content=data["text_content"],
            processed_at=data["processed_at"],
        )