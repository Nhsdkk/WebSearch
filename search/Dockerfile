FROM ghcr.io/userver-framework/ubuntu-24.04-userver:v3.2
WORKDIR /app
COPY . .
RUN cmake -S . -B /app/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSEARCH_BUILD_WEB=ON -DUSERVER_FEATURE_TESTSUITE=OFF \
    && cmake --build /app/build --parallel 4 \
    && /app/build/boolean_index data/demo.tsv data/demo.bin
ENTRYPOINT ["/app/build/search_service", "-c", "/app/service/static_config.yaml"]
