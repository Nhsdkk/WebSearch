from dataclasses import dataclass


@dataclass
class ProcessedPage:
    page_id: str
    title: str
    text_content: str
    
    def to_mongo_db(self) -> dict:
        return {
            "_id": self.page_id,
            "page_id": self.page_id,
            "title": self.title,
            "text_content": self.text_content
        }