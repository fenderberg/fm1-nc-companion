/* SPDX-License-Identifier: GPL-3.0-only */
/* Measure heavy NCC pairings with the existing regression counter and process isolation. Instruction counts
 * belong to the Mac host, not the AC791N. This is comparative evidence; it cannot certify device deadlines. */
#define main ncc_hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
static struct { double ns, ipc; } R;
static int32_t last_out[2 * CTL];
typedef struct { const uint8_t (*parts)[3]; } job_t;
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static void job_cpu(const job_t *j)
{
    static const uint8_t NOTES[8] = {48, 52, 55, 59, 60, 64, 67, 71};
    const uint8_t (*parts)[3] = j->parts;
    uint32_t p, i, k, nb = FS / CTL;
    uint64_t i0, t0;
    host_tracks_init();
    for (p = 0; p < NPART; p++) {
        host_preset(&trk[p], parts[p][0], parts[p][1]);
        trk[p].p[P_VOICE] = V_POLY;
        trk[p].p[P_SUS] = 127;
        trk[p].p[P_AMODE] = 0;
        for (i = 0; i < parts[p][2]; i++)
            trk_note_on(&trk[p], NOTES[i] + 12u * p, 100);
    }
    for (k = 0; k < nb / 2u + nb; k++) {
        if (k == nb / 2u) {
            i0 = instr_now();
            t0 = now_ns();
        }
        mix_block(last_out, CTL);
    }
    R.ns = (double)(now_ns() - t0) / (nb * CTL);
    R.ipc = i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}

static void measure(const uint8_t (*parts)[3], double *ipc, double *ns)
{
    int fd[2], status;
    pid_t pid;
    if (pipe(fd)) exit(2);
    fflush(stdout);
    pid = fork();
    if (!pid) {
        job_t job = {parts};
        close(fd[0]); job_cpu(&job);
        if (write(fd[1], &R, sizeof R) != sizeof R) _exit(2);
        _exit(0);
    }
    close(fd[1]);
    if (pid < 0 || read(fd[0], &R, sizeof R) != sizeof R) exit(2);
    close(fd[0]);
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) exit(2);
    *ipc = R.ipc; *ns = R.ns;
}
int main(void)
{
    static const uint8_t parts[][NPART][3] = {
        {{0,0,0},{0,0,0},{0,0,0},{0,0,0}},
        {{12,0,4},{12,0,4},{0,0,0},{0,0,0}},
        {{9,7,3},{8,1,3},{0,0,0},{0,0,0}},
        {{9,7,3},{9,7,3},{0,0,0},{0,0,0}},
        {{8,1,3},{8,1,3},{0,0,0},{0,0,0}},
        {{12,0,4},{9,7,3},{0,0,0},{0,0,0}},
        {{12,0,4},{8,1,3},{0,0,0},{0,0,0}},
        {{5,0,4},{5,0,4},{0,0,0},{0,0,0}},
        {{7,4,4},{11,2,4},{0,0,0},{0,0,0}},
    };
    static const char *const names[] = {"idle", "FM6+FM6", "PHYS+GRAIN", "PHYS+PHYS", "GRAIN+GRAIN",
                                         "FM6+PHYS", "FM6+GRAIN", "VOICE+VOICE", "WHEEL+NOISE"};
    uint32_t i;
    for (i = 0; i < NELEM(parts); i++) {
        double ipc, ns;
        measure(parts[i], &ipc, &ns);
        printf("ncc extra cpu: %-12s %.0f host instructions/sample, %.1f ns/sample\n", names[i], ipc, ns);
    }
    puts("ncc extra cpu: ok (host measurements; hardware listening/deadline test pending)");
    return 0;
}
