package api

import (
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"strings"

	domain "arena/server/session"

	"github.com/gin-gonic/gin"
)

type api struct {
	service *domain.SessionService
	logger  *slog.Logger
}

func NewRouter(service *domain.SessionService, logger *slog.Logger) *gin.Engine {
	router := gin.New()
	router.Use(gin.Logger(), gin.CustomRecovery(func(c *gin.Context, _ any) {
		writeError(c, http.StatusInternalServerError, "internal_error", "internal server error")
	}))
	router.HandleMethodNotAllowed = true
	h := &api{service: service, logger: logger}
	router.POST("/session", h.createSession)
	router.POST("/event", h.recordEvent)
	router.POST("/session/:id/end", h.endSession)
	router.NoRoute(func(c *gin.Context) {
		writeError(c, http.StatusNotFound, "not_found", "route not found")
	})
	router.NoMethod(func(c *gin.Context) {
		writeError(c, http.StatusMethodNotAllowed, "method_not_allowed", "method not allowed")
	})
	return router
}

func (h *api) createSession(c *gin.Context) {
	var request struct {
		PlayerName string `json:"player_name" binding:"required"`
		DeviceInfo string `json:"device_info"`
	}
	if !bindJSON(c, &request) {
		return
	}
	request.PlayerName = strings.TrimSpace(request.PlayerName)
	if request.PlayerName == "" {
		writeError(c, http.StatusBadRequest, "invalid_request", "player_name must not be blank")
		return
	}
	session, err := h.service.Create(c.Request.Context(), request.PlayerName, request.DeviceInfo)
	h.respond(c, http.StatusCreated, session, err)
}

func (h *api) recordEvent(c *gin.Context) {
	var event domain.Event
	if !bindJSON(c, &event) {
		return
	}
	if strings.TrimSpace(event.SessionID) == "" {
		writeError(c, http.StatusBadRequest, "invalid_request", "session_id must not be blank")
		return
	}
	session, err := h.service.RecordEvent(c.Request.Context(), event)
	h.respond(c, http.StatusOK, session, err)
}

func (h *api) endSession(c *gin.Context) {
	session, err := h.service.End(c.Request.Context(), c.Param("id"))
	h.respond(c, http.StatusOK, session, err)
}

// Gin caches the bounded body so json.Valid can also reject trailing JSON values.
func bindJSON(c *gin.Context, target any) bool {
	c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, 1<<20)
	if err := c.ShouldBindBodyWithJSON(target); err != nil {
		var tooLarge *http.MaxBytesError
		if errors.As(err, &tooLarge) {
			writeError(c, http.StatusRequestEntityTooLarge, "request_too_large", "JSON body exceeds 1 MiB")
		} else {
			writeError(c, http.StatusBadRequest, "invalid_request", "expected a JSON object with valid field types and required fields")
		}
		return false
	}
	body := c.MustGet(gin.BodyBytesKey).([]byte)
	if !json.Valid(body) {
		writeError(c, http.StatusBadRequest, "invalid_request", "body must contain exactly one JSON object")
		return false
	}
	return true
}

func (h *api) respond(c *gin.Context, status int, session domain.Session, err error) {
	switch {
	case err == nil:
		c.JSON(status, session)
	case errors.Is(err, domain.ErrNotFound):
		writeError(c, http.StatusNotFound, "session_not_found", err.Error())
	case errors.Is(err, domain.ErrEnded):
		writeError(c, http.StatusConflict, "session_ended", err.Error())
	case errors.Is(err, domain.ErrInvalidEvent):
		writeError(c, http.StatusBadRequest, "invalid_event_type", err.Error())
	default:
		h.logger.ErrorContext(c.Request.Context(), "session operation failed", "error", err)
		writeError(c, http.StatusInternalServerError, "internal_error", "internal server error")
	}
}

func writeError(c *gin.Context, status int, code, message string) {
	c.AbortWithStatusJSON(status, gin.H{"error": gin.H{"code": code, "message": message}})
}
