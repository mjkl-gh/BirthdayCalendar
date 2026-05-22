# syntax=docker/dockerfile:1.7

FROM node:22-alpine AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package.json ./
RUN npm install
COPY frontend/ ./
RUN npm run build

FROM alpine:3.21 AS backend-builder
RUN apk add --no-cache build-base cmake git curl-dev openssl-dev
WORKDIR /app
COPY backend/ ./backend/
RUN cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release \
  && cmake --build backend/build --config Release -j

FROM alpine:3.21 AS runtime
RUN apk add --no-cache libstdc++ libcurl openssl
WORKDIR /app
COPY --from=backend-builder /app/backend/build/birthday_backend /app/birthday_backend
COPY --from=frontend-builder /app/frontend/dist /app/public
COPY storage /app/storage
ENV PORT=8080
ENV PUBLIC_DIR=/app/public
ENV PENDING_DIR=/app/storage/pending
EXPOSE 8080
CMD ["/app/birthday_backend"]