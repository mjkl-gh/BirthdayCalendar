# syntax=docker/dockerfile:1.7

FROM node:22-alpine AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package.json ./
RUN npm install
COPY frontend/ ./
RUN npm run build && npm run build:auth

FROM alpine:3.23 AS backend-builder
# Install static/development packages for a musl static build
RUN apk add --no-cache build-base cmake git curl-dev openssl-dev musl-dev ca-certificates
WORKDIR /app
COPY backend/ ./backend/
RUN cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ \
  && cmake --build backend/build --config Release -- -j$(nproc) VERBOSE=1

# Show build directory contents for debugging (helps explain missing binary)
RUN ls -la backend/build || true
RUN ls -la backend/build/* || true

FROM alpine:3.23 AS runtime
# Runtime needs the dynamic libraries used by the binary
RUN apk add --no-cache libstdc++ libcurl openssl ca-certificates
WORKDIR /app
COPY --from=backend-builder /app/backend/build/magic_mirror_companion_backend /app/magic_mirror_companion_backend
COPY --from=frontend-builder /app/frontend/dist /app/public
COPY --from=frontend-builder /app/frontend/auth-dist /app/auth-public
COPY storage /app/storage
  # Create an unprivileged runtime user and ensure storage ownership is correct
  RUN addgroup -S app && adduser -S -G app -u 1000 app \
    && mkdir -p /app/storage/pending \
    && chown -R app:app /app/storage /app/public /app/auth-public /app/magic_mirror_companion_backend \
    && chmod +x /app/magic_mirror_companion_backend || true
  USER app
ENV PORT=8080
ENV PUBLIC_DIR=/app/public
ENV AUTH_PUBLIC_DIR=/app/auth-public
ENV PENDING_DIR=/app/storage/pending
EXPOSE 8080
CMD ["/app/magic_mirror_companion_backend"]