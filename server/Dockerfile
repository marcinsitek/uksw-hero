# Build context: ./server
FROM golang:1.27 AS build

WORKDIR /src

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 go build -trimpath -o /out/uksw-hero-server .

FROM scratch

WORKDIR /app

COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
COPY --from=build /out/uksw-hero-server /app/uksw-hero-server

USER 65532:65532

ENV ADDR=:8080 \
    GIN_MODE=release

EXPOSE 8080

ENTRYPOINT ["/app/uksw-hero-server"]
